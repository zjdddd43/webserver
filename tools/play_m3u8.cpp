#include <iostream>
#include <string>
#include <fstream>

using namespace std;

int main(int argc, char* argv[])
{
    string path = argv[1];
    string filename = path.substr(path.find_last_of("/"));

    // cout << filename << endl;
    // cout << filename.substr(0, filename.size()-5) << endl;
    // cout << "." << endl;
    // cout << filename.substr(filename.size()-4) << endl;

    string message;
    message += "<!DOCTYPE html>\r\n";
    message += "<html lang=\"en\">\r\n";
    message += "    <head>\r\n";
    message += "        <meta charset=\"UTF-8\">\r\n";
    message += "        <button type=\"submit\" onclick=\"jump()\">返回上一页</button>\r\n";
    message += "        <title>学习资料</title>\r\n";
    message += "        <link rel=\"stylesheet\" type=\"text/css\" href=\"/videojs/video-js.css\">\r\n";
    message += "        <script src=\"/videojs/video.min.js\"></script>\r\n";
    message += "        <script src=\"/videojs/videojs-contrib-hls.min.js\"></script>\r\n";
    message += "    </head>\r\n";
    message += "    <body>\r\n";
    message += "        <div align=\"center\">\r\n";
    message += "        <section id=\"videoPlayer\">\r\n";
    message += "            <video id=\"video\" width=\"600\" height=\"300\" class=\"video-js vjs-default-skin vjs-big-play-centered\" poster=\"\">\r\n";
    message += "                <source src=\"";
    message += argv[1];
    message += filename.substr(0, filename.size()-5);
    message += ".";
    message += filename.substr(filename.size()-4);
    message += "\" type=\"application/x-mpegURL\" id=\"target\">\r\n";
    message += "            </video\r\n";
    message += "        </section>\r\n";
    message += "        </div>\r\n";
    message += "        <script type=\"text/javascript\">\r\n";
    message += "            var player = videojs('video', { \"poster\": \"\", \"controls\": \"true\" }, function() {\r\n";
    message += "                this.on('play', function() {\r\n";
    message += "                });\r\n";
    message += "                this.on('pause', function() {\r\n";
    message += "                });\r\n";
    message += "                this.on('ended', function() {\r\n";
    message += "                })\r\n";
    message += "            });\r\n";
    message += "        </script>\r\n";
    message += "        <script>\r\n";
    message += "            function jump()\r\n";
    message += "            {\r\n";
    message += "                var loc = location.href;\r\n";
    message += "                loc = loc.substr(0, loc.lastIndexOf('/'));\r\n";
    message += "                window.location.href = loc;\r\n";
    message += "            }\r\n";
    message += "        </script>\r\n";
    message += "    </body>\r\n";
    message += "</html>\r\n";

    // cout << message << endl;
    ofstream out_file;
    string out_file_path = "/home/zjd/webserver/resource/videojs/play-m3u8.html";
    // cout << out_file_path << endl;
    out_file.open(out_file_path.c_str());
    out_file << message;
    out_file.close();

}