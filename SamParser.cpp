/* Copyright (c) 2017
   Bo Li (The Broad Institute of MIT and Harvard)
   libo@broadinstitute.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.   

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA
*/

#include <cstdio>
#include <cstring>
#include <cassert>
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>

#include "htslib/bgzf.h"
#include "htslib/hts.h"
#include "htslib/sam.h"

#include "my_assert.h"
#include "SamParser.hpp"

SamParser::SamParser(const char* inpF, htsThreadPool* p) {
	sam_in = sam_open(inpF, "r");
	general_assert(sam_in != 0, "Cannot open " + cstrtos(inpF) + "! It may not exist.");

	header = sam_hdr_read(sam_in);
	general_assert(header != 0, "Fail to parse the header!");

	memset(program_id, 0, sizeof(program_id));

	if (p != NULL) hts_set_opt(sam_in, HTS_OPT_THREAD_POOL, p);
}

SamParser::~SamParser() {
	bam_hdr_destroy(header);
	sam_close(sam_in);
}

// This is an simple implementation, improve it later if necessary
const char* SamParser::getProgramID() {
	if (program_id[0]) return program_id;
  
	char *p = strstr(header->text, "@PG\t");
  	assert(p != NULL);
  	p += 4;

  	char *fr = p;
  	while (*p != '\n' && *p != '\0') {
		if (*p == '\t') {
			if (set_program_id(fr, p)) return program_id;
	  		fr = p + 1;
		}
		++p;
	}
	assert(set_program_id(fr, p)); 
	return program_id;
}



void SamParser::buildMapping(const char* transListF, const bam_hdr_t* header, const char* remapF) {
	bam_hdr_t *ref_header = NULL;
	std::map<std::string, int> tname2tid; // mapping from transcript name to transcript id
	std::map<std::string, int>::iterator iter;
	std::vector<bool> appeared; // vector recording if a RSEM tid appeared in the alignment file

	// build reference header
	std::ifstream fin(transListF);
	general_assert(fin.is_open(), "Cannot open " + cstrtos(transListF) + "!");

	std::string line, tname;
	std::istringstream strin;

	ref_header = bam_hdr_init();
	assert(fin>> ref_header->n_targets);
	getline(fin, line);

	tname2tid.clear();
	ref_header->target_len = new uint32_t[ref_header->n_targets];
	ref_header->target_name = new char*[ref_header->n_targets];
	for (int i = 0; i < ref_header->n_targets; ++i) {
		assert(getline(fin, line));
		strin.clear(); strin.str(line);

		assert(getline(strin, tname, '\t'));
		assert(strin>> ref_header->target_len[i]);

		ref_header->target_name[i] = new char[tname.length() + 1];
		strncpy(ref_header->target_name[i], tname.data(), tname.length());
		ref_header->target_name[i][tname.length()] = 0;

		tname2tid[tname] = i;
	}

	appeared.assign(ref_header->n_targets, false);

	// build mapping
	int n_omit = ref_header->n_targets;
	remap = (header->n_targets != ref_header->n_targets);
	sid2tid.assign(header->n_targets, -1);

	for (int i = 0; i < header->n_targets; ++i) {
		iter = tname2tid.find(std::string(header->target_name[i]));
		general_assert(iter != tname2tid.end(), "Sequence " + cstrtos(header->target_name[i]) + " is not included in the RSEM indices!");
		general_assert(header->target_len[i] == ref_header->target_len[iter->second], "Sequence " + cstrtos(header->target_name[i]) + \
			"'s length (" + itos(header->target_len[i]) + ") in the SAM/BAM file is not consistent with the RSEM indices (" + \
			itos(ref_header->target_len[iter->second]) + ")!");

		sid2tid[i] = iter->second;
		appeared[iter->second] = true; --n_omit;
		remap = remap || (sid2tid[i] != i);
	}

	// write mapping
	std::ofstream fout(remapF);
	fout<< remap<< std::endl;
	if (remap) {
		fout<< header->n_targets;
		for (int i = 0; i < header->n_targets; ++i) fout<< " "<< sid2tid[i];
		fout<< std::endl;
		fout<< n_omit<< std::endl;
		for (int i = 0; i < ref_header->n_targets; ++i) 
			if (!appeared[i]) fout<< " "<< i + 1; // TID starts from 1
		fout<< std::endl;
	}
	fout.close();

	// release reference header
	bam_hdr_destroy(ref_header);
}

static void loadMapping(const char* remapF) {
	int n_targets;
	std::ifstream fin(remapF);
	general_assert(fin.is_open(), "Cannot open " + cstrtos(remapF) + "!");

	fin>> remap;
	if (remap) {
		fin>> n_targets; 
		sid2tid.assign(n_targets, -1);
		for (int i = 0; i < n_targets; ++i) fin>> sid2tid[i];
	}
}