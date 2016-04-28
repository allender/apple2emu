#include <string>
#include "path_utils.h"

/* 
 *  function to change the extension of the given path
*/
bool path_utils_change_ext(std::string &new_filename, const std::string &old_filename, const std::string &ext)
{
	new_filename = old_filename;
	auto location = new_filename.rfind('.');
	if (location != std::string::npos) {
		// we found the extension, so just erase
		new_filename.erase(location);
	}
	if ((new_filename.back() != '.') && (ext.front() != '.')) {
		new_filename.push_back('.');
	}
	new_filename.append(ext);
	
	return true;	
}
