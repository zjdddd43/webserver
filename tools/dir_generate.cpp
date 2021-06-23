#include <iostream>
#include <dirent.h>
#include <vector>
#include <string>
#include <fstream>

using namespace std;

int main(int argc, char* argv[])
{
    string path = argv[1];

    DIR* dir;
    dir = opendir(argv[1]);
    struct dirent* ptr;
    vector<string> file;

    while((ptr = readdir(dir)) != NULL)
    {
        if(ptr->d_name[0] == '.')
            continue;
        file.push_back(ptr->d_name);
    }
    closedir(dir);

    int pos = path.find_last_of("/");
    string name = path.substr(pos+1);
    // cout << name << endl;

    string message;
    message += "<!DOCTYPE html>\r\n";
    message += "<html lang=\"en\">\r\n";
    message += "    <head>\r\n";
    message += "        <meta charset=\"UTF-8\">\r\n";
    message += "        <title>Sign in</title>\r\n";
    message += "    </head>\r\n";
    message += "    <body>\r\n";
    message += "<br/>\r\n";
    message += "<br/>\r\n";
    message += "    <div align=\"center\">\r\n";
    message += "        <form action=\"upload\" name=\"upload_form\" id=\"upload_form\" target=\"frame1\" method=\"post\" enctype=\"multipart/form-data\">\r\n";
    message += "            <div align=\"center\"><font size=\"5\"> <strong>上传本地文件</strong></font></div>\r\n";
    message += "<br/>\r\n";
    message += "            <input type=\"file\" id=\"file\" name=\"file\">\r\n";
    message += "            <input type=\"submit\" onclick=\"upload()\" value=\"上传\">\r\n";
    message += "        </form>\r\n";
    message += "    </div>\r\n";
    message += "    <div align=\"center\">\r\n";
    message += "        <iframe name=\"frame1\"  frameborder=\"0\" height=\"50\"></iframe>\r\n";
    message += "    </div>\r\n";
    message += "<br/>\r\n";
    message += "<br/>\r\n";
    message += "    <div align=\"center\"><font size=\"5\"> <strong>刷新页面</strong></font></div>\r\n";
    message += "<br/>\r\n";
    message += "    <div align=\"center\">\r\n";
    message += "        <button type=\"submit\" onclick=\"flush()\">刷新</button>\r\n";
    message += "    </div>\r\n";
    message += "<br/>\r\n";
    message += "    <div align=\"center\"><font size=\"5\"> <strong>目录</strong></font></div>\r\n";
    message += "<br/>\r\n";
    message += "        <div class=\"login\">\r\n";

    for(int i = 0; i <file.size(); i++)
    {
        // cout << file[i] << endl;
        message += "            <div align=\"center\">\r\n";
        message += "                <a href=\"https://192.168.3.134:12345/login/";
        message += name;
        message += "/";
        message += file[i];
        message += "\">";
        message += file[i];
        message += "</button>\r\n";
        message += "            </div>\r\n";
    }
    
    message += "        </div>\r\n";
    message += "    </body>\r\n";
    message += "    <script>\r\n";
    message += "        function flush()\r\n";
    message += "        {\r\n";
    message += "            window.location.href = location.href\r\n";
    message += "        }\r\n";
    message += "        function upload()\r\n";
    message += "        {\r\n";
    message += "            $(\"#upload_form\").submit();\r\n";
    message += "            var t = setInterval(function() {\r\n";
    message += "                var word = $(\"iframe[name='frame1']\").contents().find(\"body\").text();\r\n";
    message += "                if (word != \"\") {\r\n";
    message += "                    alert(word);\r\n";
    message += "                    clearInterval(t);\r\n";
    message += "                }\r\n";
    message += "            }, 1000);\r\n";
    message += "        }\r\n";
    message += "    </script>\r\n";
    message += "</html>\r\n";

    ofstream out_file;
    string out_file_path = path+"/dir.html";
    out_file.open(out_file_path.c_str());
    out_file << message;
    out_file.close();

    return 0;
}