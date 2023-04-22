#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>

using namespace cv;
using namespace std;

class Province
{
public:
    string colour;
    Point pos;
    Mat shape;
    vector<int> neighbours;
    string subregion;
    string region;
    string continent;
    Province(string colour, Point pos)
    {
        this->colour=colour;
        this->pos=pos;
    }
};

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

bool overlap(Mat im1, Mat im2)
{
    Mat shape;
    bitwise_and(im1,im2,shape);
    return countNonZero(shape)>0;
}

Point firstNonZero(Mat img,Point start=Point(0,0))
{
    int rows=img.rows,cols=img.cols;

    for(int i=start.y;i<rows;i++)
    {
        const uchar* ptr8=img.ptr(i);
        for(int j=0;j<cols;j++)
        {
            if(ptr8[j]!=0)
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


int main(int argc, char* argv[])
{
    //Help
    if (argc>1)
    {
        string help=argv[1];
        if (help=="--help" || help=="-help" || help=="-h")
        {
            cout<<endl;
            cout<<endl;
            cout<<"Description:"<<endl;
            cout<<"\tThis program interprets maps and calculates their provinces' connections\n\t(as well as subregions, regions and continents if necessary)."<<endl;
            cout<<"\tIt outputs the provinces' owners (column 1), positions(column 2 and 3),\n\tsubregions (col 4), regions (col 5) and lastly their neighbours (all the rest)."<<endl;
            cout<<"\tAlso it has a few default flags which can be configured in default_flags.txt."<<endl;
            cout<<endl;
            cout<<"Usage:"<<endl;
            cout<<"\t-v/--verbose\t\t\tWhether to give a detailed output where the program is currently"<<endl<<endl;
            cout<<"\t--path=<PATH>\t\t\tBasically where your maps are"<<endl<<endl;
            cout<<"\t--file-extension=<EXTENSION>\tWhat file extension they use (eg. .png, .jpg or nothing at all)"<<endl<<endl;
            cout<<"\t--maximum-distance=<INTEGER>\tMaximum distance of provinces (to check neighbours)"<<endl<<endl;
            cout<<"\t--<include/exclude>-owner\tWhether to output the colours of the provinces' owners"<<endl<<endl;
            cout<<"\t--<include/exclude>-water\tWhether to calculate the neighbours of waters (water colour will be the first\n\t\t\t\t\tpixel of the upper left corner)"<<endl<<endl;
            cout<<"\t--<subregions/no-subregions>\tWhether to include subregions in the output or not"<<endl<<endl;
            cout<<"\t--<regions/no-regions>\t\tWhether to include regions in the output or not"<<endl<<endl;
            cout<<"\t--<continents/no-continents>\tWhether to include continents in the output or not"<<endl<<endl;
            cout<<"\t--<numbering/no-numbering>\tWhether to write province index into the output"<<endl<<endl;
            cout<<"\t--use-<hex/rgb>\t\t\tThe colour format of the output";
            cout<<endl;
            cout<<endl;
            return 0;
        }
    }
    //Setup flags
    bool flags[7];
    string FLAGS[7]={"owner","water","regions","subregions","continents","numbering","verbose"};
    string path,file_extension;
    int use=-1,maximum_distance=1000;
    string USE[4]={"hex","rgb"};

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
        else if (spl[0]=="maximum-distance")
        {
            maximum_distance=stoi(spl[1]);
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
                    for (int i=0;i<6;++i)
                    {
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
    while (true)
    {
        pos=firstNonZero(thresh,start);
        if (pos.x<0 || pos.y<0)
        {
            break;
        }
        start=pos;

        provinces.push_back(Province(bgr_to_hex(img.at<Vec3b>(pos)),pos));

        //If province is not water get its shape
        if (flags[1] || provinces[provinces.size()-1].colour!=provinces[0].colour)
        {
            floodFill(thresh, pos, Scalar(1));
            Mat shape;
            inRange(thresh, Scalar(1), Scalar(1), shape);
            dilate(shape,shape,Mat());
            provinces[provinces.size()-1].shape=shape;
        }
        floodFill(thresh, pos, Scalar(0));
        if (flags[6]) cout<<"\rProcessing provinces ("<<provinces.size()<<")";
    }

    thresh.release();
    img.release();

    if (flags[6]) cout<<"\rFinding neighbours...        "<<endl;
    //Adding neighbours
    int SIZE=provinces.size();
    char neighbours[SIZE][SIZE]={};

    for (int i=SIZE-1;i>=0;--i)
    {
        //Only going with j until we reach the diagonal
        for (int j=0;j<i;++j)
        {
            if (norm(provinces[i].pos-provinces[j].pos)<=maximum_distance && (flags[1] || provinces[j].colour!=provinces[0].colour) && overlap(provinces[i].shape,provinces[j].shape))
            {
                ++neighbours[i][j];
                ++neighbours[j][i];
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
            if (neighbours[i][j]==1)
            {
                provinces[i].neighbours.push_back(j);
            }
        }
    }

    for (int i=0;i<SIZE;i++)
    {
        provinces[i].shape.release();
    }

    if (flags[6]) cout<<"Finding subregions, regions and continents..."<<endl;
    //Finding regions, subregions and continents
    Mat reg,sub,cont;
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
    if (flags[4])
    {
        cont=imread(path+"continents."+file_extension, IMREAD_COLOR);
        if(cont.empty())
        {
            cout<<"Error: Could not read the image `"+path+"continents."+file_extension+"`!"<<endl;
            return 4;
        }
    }
    for (int i=0;i<SIZE;i++)
    {
        if (flags[2]) provinces[i].region=bgr_to_hex(reg.at<Vec3b>(provinces[i].pos));
        if (flags[3]) provinces[i].subregion=bgr_to_hex(sub.at<Vec3b>(provinces[i].pos));
        if (flags[4]) provinces[i].continent=bgr_to_hex(cont.at<Vec3b>(provinces[i].pos));
    }

    if (flags[6]) cout<<"Writing results to map_data.txt..."<<endl;
    //Showing the results
    ofstream out("map_data.txt");
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

    return 0;
}
