// Copyright (c) 2006-2013, Andrey N. Sabelnikov, www.sabelnikov.net
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// * Neither the name of the Andrey N. Sabelnikov nor the
// names of its contributors may be used to endorse or promote products
// derived from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER  BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 

#include "file_io_utils.h"

#include <fstream>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#ifdef _WIN32
#include <windows.h>
#include "string_tools.h"
#endif

// On Windows there is a problem with non-ASCII characters in path and file names
// as far as support by the standard components used is concerned:

// The various file stream classes, e.g. std::ifstream and std::ofstream, are
// part of the GNU C++ Library / libstdc++. On the most basic level they use the
// fopen() call as defined / made accessible to programs compiled within MSYS2
// by the stdio.h header file maintained by the MinGW project.

// The critical point: The implementation of fopen() is part of MSVCRT, the
// Microsoft Visual C/C++ Runtime Library, and this method does NOT offer any
// Unicode support.

// Monero code that would want to continue to use the normal file stream classes
// but WITH Unicode support could therefore not solve this problem on its own,
// but 2 different projects from 2 different maintaining groups would need changes
// in this particular direction - something probably difficult to achieve and
// with a long time to wait until all new versions / releases arrive.

// Implemented solution approach: Circumvent the problem by stopping to use std
// file stream classes on Windows and directly use Unicode-capable WIN32 API
// calls. Most of the code doing so is concentrated in this header file here.

namespace epee
{
namespace file_io_utils
{
 
	bool is_file_exist(const std::string& path)
	{
		boost::filesystem::path p(path);
		return boost::filesystem::exists(p);
	}


	bool save_string_to_file(const std::string& path_to_file, const std::string& str)
	{
#ifdef _WIN32
                std::wstring wide_path;
                try { wide_path = string_tools::utf8_to_utf16(path_to_file); } catch (...) { return false; }
                HANDLE file_handle = CreateFileW(wide_path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (file_handle == INVALID_HANDLE_VALUE)
                    return false;
                DWORD bytes_written;
                DWORD bytes_to_write = (DWORD)str.size();
                BOOL result = WriteFile(file_handle, str.data(), bytes_to_write, &bytes_written, NULL);
                CloseHandle(file_handle);
                if (bytes_written != bytes_to_write)
                    result = FALSE;
                return result;
#else
		try
		{
			std::ofstream fstream;
			fstream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
			fstream.open(path_to_file, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
			fstream << str;
			fstream.close();
			return true;
		}

		catch(...)
		{
			return false;
		}
#endif
	}

	bool load_file_to_string(const std::string& path_to_file, std::string& target_str, size_t max_size)
	{
#ifdef _WIN32
                std::wstring wide_path;
                try { wide_path = string_tools::utf8_to_utf16(path_to_file); } catch (...) { return false; }
                HANDLE file_handle = CreateFileW(wide_path.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (file_handle == INVALID_HANDLE_VALUE)
                    return false;
                DWORD file_size = GetFileSize(file_handle, NULL);
                if ((file_size == INVALID_FILE_SIZE) || (uint64_t)file_size > (uint64_t)max_size) {
                    CloseHandle(file_handle);
                    return false;
                }
                target_str.resize(file_size);
                DWORD bytes_read;
                BOOL result = ReadFile(file_handle, &target_str[0], file_size, &bytes_read, NULL);
                CloseHandle(file_handle);
                if (bytes_read != file_size)
                    result = FALSE;
                return result;
#else
		try
		{
			std::ifstream fstream;
			fstream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
			fstream.open(path_to_file, std::ios_base::binary | std::ios_base::in | std::ios::ate);

			std::ifstream::pos_type file_size = fstream.tellg();
			
			if((uint64_t)file_size > (uint64_t)max_size) // ensure a large domain for comparison, and negative -> too large
				return false;//don't go crazy
			size_t file_size_t = static_cast<size_t>(file_size);

			target_str.resize(file_size_t);

			fstream.seekg (0, std::ios::beg);
			fstream.read((char*)target_str.data(), target_str.size());
			fstream.close();
			return true;
		}

		catch(...)
		{
			return false;
		}
#endif
	}

}
}
