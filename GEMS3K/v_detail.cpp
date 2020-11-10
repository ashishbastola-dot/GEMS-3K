#include <cstring>
#include <istream>
#include "v_detail.h"

TError::~TError()
{}

[[ noreturn ]] void Error (const std::string& title, const std::string& message)
{
    throw TError(title, message);
}

void ErrorIf (bool error, const std::string& title, const std::string& message)
{
    if(error)
        throw TError(title, message);
}


void strip(std::string& str)
{
  std::string::size_type pos1 = str.find_first_not_of(' ');
  std::string::size_type pos2 = str.find_last_not_of(' ');
  str = str.substr(pos1 == std::string::npos ? 0 : pos1,
    pos2 == std::string::npos ? str.length() - 1 : pos2 - pos1 + 1);
}

void replace( std::string& str, const char* old_part, const char* new_part)
{
    size_t pos = str.find( old_part ); //rfind( old_part );
    if( pos != std::string::npos )
    {
        std::string res(str.substr(0, pos));
        res += new_part;
        res += str.substr( pos+strlen(old_part));
        str = res;
    }
}

void replaceall(std::string& str, char ch1, char ch2)
{
  for(size_t ii=0; ii<str.length(); ii++ )
   if( str[ii] == ch1 )
            str[ii] = ch2;
}

/// read string as: "<characters>"
std::istream& f_getline(std::istream& is, std::string& str, char delim)
{
    char ch;
    is.get(ch);
    str="";

    while( is.good() && ( ch==' ' || ch=='\n' || ch== '\t') )
        is.get(ch);
    if(ch == '\"')
        is.get(ch);
    while( is.good() &&  ch!=delim && ch!= '\"' )
    {
        str += ch;
        is.get(ch);
    }
    while( is.good() &&  ch!=delim )
            is.get(ch);

   return is;
}

std::string
u_makepath(const std::string& dir,  const std::string& name, const std::string& ext)
{
    std::string Path(dir);
    if( dir != "")
      Path += "/";
    Path += name;
    Path += ".";
    Path += ext;

    return Path;
}

std::string u_getpath( const std::string& file_path )
{
    std::size_t pos = file_path.find_last_of("/\\");
    if( pos != std::string::npos )
        return file_path.substr(0, pos);
    return "";
}


void u_splitpath(const std::string& Path, std::string& dir,
            std::string& name, std::string& ext)
{
    // Get path
    std::size_t pos = Path.find_last_of("/\\");
    if( pos != std::string::npos )
    {
        dir = Path.substr(0, pos);
        pos++;
    }
    else
    {
        dir = "";
        pos = 0;
    }
    name = Path.substr(pos);
    size_t pose = name.rfind(".");
    if( pose != std::string::npos )
    {
        ext = name.substr( pose+1, std::string::npos );
        name = name.substr(0, pose);
    }
    else
    {
        ext = "";
    }
}
