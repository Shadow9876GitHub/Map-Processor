#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <time.h>
#include <thread>
#include <mutex>

using namespace cv;
using namespace std;

#define F_SIZE 10

//Mutex
mutex m;

//Setup flags
bool flags[F_SIZE]={true,false,false,false,false,false,false,false,false,true};
string FLAGS[F_SIZE]={"owner","water","regions","subregions","continents","numbering","verbose","countries","empires","headers"};
string path, file_extension="png";
int use=0;
int cores=6;
string USE[2]={"hex","rgb"};
string water_colour;

class Province
{
public:
    int index;
    string colour;
    Point pos;
    vector<uchar> shape;
    vector<int> neighbours;
    string subregion;
    string region;
    string country;
    string empire;
    string continent;
    Rect box;
    Province(int index, string colour, Point posm, Rect box)
    {
        this->index=index;
        this->colour=colour;
        this->pos=pos;
        this->box=box;
    }
    Province(int index, string colour, Point pos, vector<uchar> shape, Rect box)
    {
        this->index=index;
        this->colour=colour;
        this->pos=pos;
        this->shape=shape;
        this->box=box;
    }
    bool operator< (const Province &other) const {
        return index < other.index;
    }
};

void showImage(Mat image, string window_name="image")
{
    namedWindow(window_name, WINDOW_AUTOSIZE);
    imshow(window_name, image);
    waitKey(0);
    destroyAllWindows();
}

string bgr_to_hex(Vec3b colour)
{
    string chrs="0123456789ABCDEF";
    string hexa="#";
    for (int i=2;i>=0;--i)
    {
        hexa+=chrs[colour[i]/16];
        hexa+=chrs[colour[i]%16];
    }
    return hexa;
}

bool overlap(vector<uchar> c1, Rect b1, vector<uchar> c2, Rect b2)
{
    Mat im1,im2;
    im1=imdecode(c1,IMREAD_GRAYSCALE );
    im2=imdecode(c2,IMREAD_GRAYSCALE );
    Mat shape;
    Mat im3, im4;
    int x,y,w,h;
    x=min(b1.x,b2.x);y=min(b1.y,b2.y);w=max(b1.x+b1.width,b2.x+b2.width);h=max(b1.y+b1.height,b2.y+b2.height);
    copyMakeBorder(im1, im3, b1.y-y, h-b1.y-b1.height, b1.x-x, w-b1.x-b1.width, BORDER_CONSTANT, Scalar(0));
    copyMakeBorder(im2, im4, b2.y-y, h-b2.y-b2.height, b2.x-x, w-b2.x-b2.width, BORDER_CONSTANT, Scalar(0));
    bitwise_and(im3,im4,shape);
    return countNonZero(shape)>0;
}

bool do_intersect(Rect a, Rect b)
{
    if (a.x>b.x+b.width || b.x>a.x+a.width || a.y>b.y+b.height || b.y>a.y+a.height)
    {
        return false;
    }
    return true;
}

Point firstNonZero(Mat img,Point start=Point(0,0))
{
    int rows=img.rows,cols=img.cols;

    for(int i=start.y;i<rows;i++)
    {
        const uchar* ptr8=img.ptr(i);
        for(int j=0;j<cols;j++)
        {
            if(ptr8[j]==255)
            {
                return Point(j,i);
            }
        }
    }
    return Point(-1,-1);
}

vector<string> custom_split(string str, char del)
{
    vector<string> res;
    string curr="";
    for (int i=0;i<str.length();++i)
    {
        if (str[i]==del)
        {
            res.push_back(curr);
            curr="";
        }
        else
        {
            curr+=str[i];
        }
    }
    res.push_back(curr);
    return res;
}

void get_province(int index, Mat img, Point pos, vector<Province> &provinces, Mat thresh, int thread_num)
{
    Mat range, shape, cropped;
    vector<uchar> buff;

    vector<int> param(2);
    param[0]=IMWRITE_JPEG_QUALITY;
    param[1]=89;
    Rect box;
    Rect rect;

    inRange(thresh, Scalar(thread_num), Scalar(thread_num), range);
    dilate(range,shape,Mat());

    box=boundingRect(shape);

    //If province is not water get its shape
    if (flags[1] || bgr_to_hex(img.at<Vec3b>(pos))!=water_colour)
    {
        cropped=shape(Range(box.y,box.y+box.height),Range(box.x,box.x+box.width));

        imencode(".jpg", cropped, buff, param); //Compressing image in memory
        m.lock();
        provinces.push_back(Province(index, bgr_to_hex(img.at<Vec3b>(pos)),pos,buff,box));
        m.unlock();
    }
    else
    {
        m.lock();
        provinces.push_back(Province(index, bgr_to_hex(img.at<Vec3b>(pos)),pos,box));
        m.unlock();
    }
    floodFill(thresh, pos, Scalar(0), &rect, Scalar(0), Scalar(0), 8);
}

int main(int argc, char* argv[])
{
    clock_t Start=clock();
    //Help
    if (argc>1)
    {
        string help=argv[1];
        if (help=="--help" || help=="-help" || help=="-h" || help=="help" || help=="h")
        {
            cout<<endl<<endl;
            cout<<"Description:"<<endl;
            cout<<"\tThis program interprets maps and calculates their provinces' connections\n\t(as well as subregions, regions and continents if necessary)."<<endl;
            cout<<"\tBy default it outputs the provinces' owners (column 1), positions(column 2 and 3),\n\tsubregions (col 4), regions (col 5) and lastly their neighbours (all the rest) but these can be changed."<<endl;
            cout<<"\tAlso it has a few default flags which can be configured in default_flags.txt."<<endl;
            cout<<endl;
            cout<<"Usage:"<<endl;
            cout<<"\t-v/--verbose\t\t\tWhether to give a detailed output where the program is currently"<<endl<<endl;
            cout<<"\t--path=<PATH>\t\t\tBasically where your maps are"<<endl<<endl;
            cout<<"\t--file-extension=<EXTENSION>\tWhat file extension they use (eg. .png, .jpg or nothing at all)"<<endl<<endl;
            cout<<"\t--water-colour=<HEX>\t\tThe water colour in hex, if nothing is given it defaults to the colour at\n\t\t\t\t\tthe top left corner of the image"<<endl<<endl;
            cout<<"\t--owner\t\t\t\tWhether to output the colours of the provinces' owners"<<endl<<endl;
            cout<<"\t--water\t\t\t\t\Whether to calculate the neighbours of waters (water colour will be the first\n\t\t\t\t\tpixel of the upper left corner)"<<endl<<endl;
            cout<<"\t--subregions\t\t\tWhether to include subregions in the output or not"<<endl<<endl;
            cout<<"\t--regions\t\t\tWhether to include regions in the output or not"<<endl<<endl;
            cout<<"\t--countries\t\t\tWhether to include countries in the output or not"<<endl<<endl;
            cout<<"\t--empires\t\t\tWhether to include empires in the output or not"<<endl<<endl;
            cout<<"\t--continents\t\t\tWhether to include continents in the output or not"<<endl<<endl;
            cout<<"\t--numbering\t\t\tWhether to write province index into the output"<<endl<<endl;
            cout<<"\t--use-<hex/rgb>\t\t\tThe colour format of the output"<<endl<<endl;
            cout<<"\t--cores=<VALUE>\t\t\tThe number of CPU cores the application should use"<<endl<<endl;
            cout<<"\t--headers\t\t\tWhether to use headers to label the information (it's automatically on,\n\t\t\t\t\ttype no-headers if you don't want to use them)";
            cout<<endl<<endl;
            return 0;
        }
    }
    //Helper to flags
    string line;
    vector<string> spl;

    ifstream inp("default_flags.txt");
    int ll=0;

    //Read flags
    while (getline(inp, line) || (++ll)<argc)
    {
        if (ll>=1)
        {
            line=argv[ll];
            while (line[0]=='-')
            {
                line.erase(0,1);
            }
        }
        spl=custom_split(line,'=');
        if (line=="verbose" || line=="v")
        {
            flags[6]=true;
        }
        else if (spl[0]=="file-extension")
        {
            file_extension=spl[1];
        }
        else if (spl[0]=="water-colour" || spl[0]=="water-color")
        {
            water_colour=spl[1];
        }
        else if (spl[0]=="cores")
        {
            cores=stoi(spl[1]);
        }
        else
        {
            if (spl[0]=="path")
            {
                path=spl[1];
            }
            else
            {
                spl=custom_split(line,'-');
                if (spl[0]=="use")
                {
                    for (int i=0;i<4;++i)
                    {
                        if (spl[1]==USE[i])
                        {
                            use=i;
                            break;
                        }
                    }
                }
                else
                {
                    for (int i=0;i<F_SIZE;++i)
                    {
                        if (i==6) continue;
                        if (spl[1]==FLAGS[i] || spl[0]==FLAGS[i])
                        {
                            flags[i]=!(spl[0]=="no" || spl[0]=="exclude");
                            break;
                        }
                    }
                }
            }
        }
    }

    if (flags[6]) cout<<"Reading image..."<<endl;
    Mat img=imread(path+"map."+file_extension, IMREAD_COLOR);
    if(img.empty())
    {
        cout<<"Error: Could not read the image `"+path+"map."+file_extension+"`!"<<endl;
        return 1;
    }
    if (water_colour.empty())
    {
        water_colour=bgr_to_hex(img.at<Vec3b>(Point(0,0)));
    }
    //Not even starting if regions, subregions or continents are missing if appropriate flags are set
    Mat reg,sub,cou,emp,cont;
    if (flags[2])
    {
        reg=imread(path+"regions."+file_extension, IMREAD_COLOR);
        if(reg.empty())
        {
            cout<<"Error: Could not read the image `"+path+"regions."+file_extension+"`!"<<endl;
            return 2;
        }
    }
    if (flags[3])
    {
        sub=imread(path+"subregions."+file_extension, IMREAD_COLOR);
        if(sub.empty())
        {
            cout<<"Error: Could not read the image `"+path+"subregions."+file_extension+"`!"<<endl;
            return 3;
        }
    }
    if (flags[7])
    {
        cou=imread(path+"countries."+file_extension, IMREAD_COLOR);
        if(cou.empty())
        {
            cout<<"Error: Could not read the image `"+path+"countries."+file_extension+"`!"<<endl;
            return 7;
        }
    }
    if (flags[8])
    {
        emp=imread(path+"empires."+file_extension, IMREAD_COLOR);
        if(emp.empty())
        {
            cout<<"Error: Could not read the image `"+path+"empires."+file_extension+"`!"<<endl;
            return 8;
        }
    }
    if (flags[4])
    {
        cont=imread(path+"continents."+file_extension, IMREAD_COLOR);
        if(cont.empty())
        {
            cout<<"Error: Could not read the image `"+path+"continents."+file_extension+"`!"<<endl;
            return 4;
        }
    }
    if (flags[6]) cout<<"Preparing greyscale images..."<<endl;
    //Making greyscale image
    Mat greyImage;
    cvtColor(img, greyImage, COLOR_BGR2GRAY);
    //Making black and white image
    Mat thresh;
    threshold(greyImage,thresh,0,255,THRESH_BINARY);

    greyImage.release();

    vector<Province> provinces;

    if (flags[6]) cout<<"Getting basic information..."<<endl;
    //Getting basic information of the map (position and colour)
    Point pos;
    Point start=Point(0,0);
    Rect rect;
    //vector<Point> positions=allNonZero(thresh);
    int i=-1;
    bool loop=true;
    if (flags[6]) cout<<"Using "<<cores<<" cores..."<<endl;
    while (loop)
    {
        vector<thread> pool;
        for (int j=0;j<cores;j++)
        {
            ++i;
            pos=firstNonZero(thresh,start);
            if (pos.x<0 || pos.y<0)
            {
                loop=false;
                break;
            }
            start=pos;
            floodFill(thresh, pos, Scalar(j+1), &rect, Scalar(0), Scalar(0), 8);
            pool.push_back(thread(get_province,i,img,pos,ref(provinces),thresh,j+1));
        }
        for (auto& j : pool)
        {
            j.join();
        }
        //get_province(img,positions[i],provinces,thresh);

        if (flags[6]) cout<<"\rProcessing provinces ("<<provinces.size()<<")";
    }
    sort(provinces.begin(),provinces.end());

    thresh.release();
    img.release();

    if (flags[6]) cout<<"\r"<<provinces.size()<<" provinces processed...        "<<endl;

    if (flags[6]) cout<<"Finding neighbours..."<<endl;
    //Adding neighbours
    int SIZE=provinces.size();
    bool **neighbours = new bool*[SIZE];
    for(int i=0;i<SIZE;++i)
    {
        neighbours[i]=new bool[SIZE]{false};
    }

    for (int i=SIZE-1;i>=0;--i)
    {
        if (flags[1] || provinces[i].colour!=water_colour)
        {
            //Only going with j until we reach the diagonal
            for (int j=0;j<i;++j)
            {
                if (do_intersect(provinces[i].box, provinces[j].box) && (flags[1] || provinces[j].colour!=water_colour) && overlap(provinces[i].shape,provinces[i].box,provinces[j].shape,provinces[j].box))
                {
                    neighbours[i][j]=true;
                    neighbours[j][i]=true;
                }
            }
        }

        if (flags[6]) cout<<"\r"<<SIZE-i<<"/"<<SIZE<<" province done";
    }
    if (flags[6]) cout<<"\rAssigning found neighbours..."<<endl;
    //Assigning neighbours based on the "graph"
    for (int i=0;i<SIZE;++i)
    {
        for (int j=0;j<SIZE;++j)
        {
            if (neighbours[i][j])
            {
                provinces[i].neighbours.push_back(j);
            }
        }
    }

    for(int i=0;i<SIZE;++i)
    {
        delete [] neighbours[i];
    }
    delete [] neighbours;

    if (flags[6] && (flags[2] || flags[3] || flags[4] || flags[7] || flags[8])) cout<<"Finding other map data (subregions, regions, continents...)"<<endl;
    //Finding regions, subregions and continents
    for (int i=0;i<SIZE;i++)
    {
        if (flags[2]) provinces[i].region=bgr_to_hex(reg.at<Vec3b>(provinces[i].pos));
        if (flags[3]) provinces[i].subregion=bgr_to_hex(sub.at<Vec3b>(provinces[i].pos));
        if (flags[7]) provinces[i].country=bgr_to_hex(cou.at<Vec3b>(provinces[i].pos));
        if (flags[8]) provinces[i].empire=bgr_to_hex(emp.at<Vec3b>(provinces[i].pos));
        if (flags[4]) provinces[i].continent=bgr_to_hex(cont.at<Vec3b>(provinces[i].pos));
    }

    if (flags[6]) cout<<"Writing results to map_data.txt..."<<endl;
    //Showing the results
    ofstream out("map_data.txt");
    if (flags[9])
    {
        if (flags[5]) out<<"index ";
        if (flags[0]) out<<"colour ";
        out<<"position ";
        out<<"box ";
        if (flags[3]) out<<"subregion ";
        if (flags[2]) out<<"region ";
        if (flags[7]) out<<"country ";
        if (flags[8]) out<<"empire ";
        if (flags[4]) out<<"continent ";
        out<<"neighbours\n";
    }
    for (int i=0;i<SIZE;i++)
    {
        if (flags[5]) out<<i<<" ";
        if (flags[0])
        {
            if (use==0) out<<provinces[i].colour<<" ";
            else
            {
                int r,g,b;
                char const *hexColor = provinces[i].colour.c_str();
                sscanf(hexColor, "#%02x%02x%02x", &r, &g, &b);
                out<<r<<" "<<g<<" "<<b<<" ";
            }
        }
        out<<provinces[i].pos.x<<" "<<provinces[i].pos.y<<" ";
        out<<provinces[i].box.x<<" "<<provinces[i].box.y<<" "<<provinces[i].box.height<<" "<<provinces[i].box.width<<" ";
        if (flags[3])
        {
            if (use==0) out<<provinces[i].subregion<<" ";
            else
            {
                int r,g,b;
                char const *hexColor = provinces[i].subregion.c_str();
                sscanf(hexColor, "#%02x%02x%02x", &r, &g, &b);
                out<<r<<" "<<g<<" "<<b<<" ";
            }
        }
        if (flags[2])
        {
            if (use==0) out<<provinces[i].region<<" ";
            else
            {
                int r,g,b;
                char const *hexColor = provinces[i].region.c_str();
                sscanf(hexColor, "#%02x%02x%02x", &r, &g, &b);
                out<<r<<" "<<g<<" "<<b<<" ";
            }
        }
        if (flags[7])
        {
            if (use==0) out<<provinces[i].country<<" ";
            else
            {
                int r,g,b;
                char const *hexColor = provinces[i].country.c_str();
                sscanf(hexColor, "#%02x%02x%02x", &r, &g, &b);
                out<<r<<" "<<g<<" "<<b<<" ";
            }
        }
        if (flags[8])
        {
            if (use==0) out<<provinces[i].empire<<" ";
            else
            {
                int r,g,b;
                char const *hexColor = provinces[i].empire.c_str();
                sscanf(hexColor, "#%02x%02x%02x", &r, &g, &b);
                out<<r<<" "<<g<<" "<<b<<" ";
            }
        }
        if (flags[4])
        {
            if (use==0) out<<provinces[i].continent<<" ";
            else
            {
                int r,g,b;
                char const *hexColor = provinces[i].continent.c_str();
                sscanf(hexColor, "#%02x%02x%02x", &r, &g, &b);
                out<<r<<" "<<g<<" "<<b<<" ";
            }
        }
        for (int j=0;j<provinces[i].neighbours.size();++j)
        {
            out<<provinces[i].neighbours[j]<<" ";
        }
        out<<"\n";
    }

    if (flags[6])
    {
        cout<<endl<<"Execution time: "<<(double)(clock()-Start)/CLOCKS_PER_SEC<<endl;
    }

    return 0;
}
