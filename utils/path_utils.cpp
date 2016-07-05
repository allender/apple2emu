/*

MIT License

Copyright (c) 2016 Mark Allender


Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

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

void path_utils_get_filename(std::string &full_path, std::string &filename)
{
	int extension_loc = full_path.find_last_of('.');
	int path_loc = full_path.find_last_of('\\');
	if (path_loc == std::string::npos) {
		path_loc = full_path.find_last_of('/');
	}

	// no path separator or extension
	if (path_loc == std::string::npos && extension_loc == std::string::npos) {
		filename = full_path;
	}

	// no path separator, but extension
	else if (path_loc == std::string::npos) {
		filename = full_path.substr(0, extension_loc - 1);
	}

	// path separator and extensiona
	else {
		filename = full_path.substr(path_loc + 1, extension_loc - 1);
	}
}
