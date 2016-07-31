/*
 * Copyright (c) 2016, Kelvin W Sherlock
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "native_internal.h"

#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#define XATTR_FINDERINFO_NAME	"com.apple.FinderInfo"
#define XATTR_RESOURCEFORK_NAME	"com.apple.ResourceFork"

#define XATTR_FILETYPE_NAME	"prodos.FileType"
#define XATTR_AUXTYPE_NAME	"prodos.AuxType"


using namespace MacOS;

namespace {

	uint32_t rforksize(const std::string &path_name)
	{
		int fd;
		uint32_t rv = 0;
		struct stat st;

			fd = attropen(path_name.c_str(), XATTR_RESOURCEFORK_NAME, O_RDONLY);
			if (fd >= 0) {

			if (fstat(fd, &st) == 0) rv = st.st_size;

			close(fd);
		}
		return rv;
	}

}

namespace native {


	macos_error get_finder_info(const std::string &path_name, void *info, bool extended) {

		uint8_t buffer[32];
		std::memset(buffer, 0, sizeof(buffer));

		int fd;
		fd = attropen(path_name.c_str(), XATTR_FINDERINFO_NAME, O_RDONLY);
		if (fd >= 0) {
			ssize_t x = read(fd, buffer, 32);
			close(fd);
			if (x == 32 || x == 16){
				fixup_prodos_ftype(buffer);
				memcpy(info, buffer, extended ? 32 : 16);
				return 0;
			}
		}

		/* if it's a text file, call it a text file */
		if (is_text_file_internal(path_name)) {
			memcpy(buffer, "TEXTMPS ", 8);
		}

		memcpy(info, buffer, extended ? 32 : 16);
		return noErr;

	}

	macos_error get_file_info(const std::string &path_name, file_info &fi)
	{
		struct stat st;

		if (stat(path_name.c_str(), &st) < 0)
			return macos_error_from_errno();

		fi.create_date = unix_to_mac(st.st_ctime);
		fi.modify_date = unix_to_mac(st.st_mtime);
		fi.backup_date = 0;

		if (S_ISDIR(st.st_mode)) {
			fi.type = file_info::directory;


			int links = st.st_nlink - 2;
			if (links < 0) links = 0;
			if (links > 65535) links = 65535;

			fi.entry_count = links;
			return noErr;
		}

		// todo -- get actual block size instead of assuming 512.  oh well!
		
		fi.type = file_info::file;
		fi.data_logical_size = st.st_size;
		fi.data_physical_size = (st.st_size + 511) & ~511;
		fi.resource_physical_size = 0;
		fi.resource_logical_size = 0;

		get_finder_info(path_name, fi.finder_info);

		ssize_t rsize = rforksize(path_name);
		if (rsize > 0) {
			fi.resource_physical_size = rsize;
			fi.resource_logical_size = (rsize + 511) & ~511;
		}

		return noErr;
	}



}