/*
 * nhocr-hcl.cpp
 *
 *  Created on: Jan 30, 2012
 *      Author: asanoki
 */

#include <fstream>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <map>
#include <istream>
#include <sstream>
#include <iterator>

int main(int argc, char **argv) {
	std::fstream input;
	std::fstream mapping;
	std::fstream output_cctable;
	std::fstream output;
	std::vector<unsigned short> codes;
	std::map<unsigned short, std::string> mapping_to_utf8;
	char buffer[16] = { 0 };
	char buffer_zero_before[512] = { 0 };
	char buffer_zero_after[520] = { 0 };
	int sheets_per_set = 20;
	int embedded_sets = 2;
	if (argc < 4) {
		std::cerr
				<< "Usage: ./nhocr-etl9b <input-etl9b-file> <output-img-file-prefix> <output-cctable-file> [sheets-in-each-set, default:20] [embedded-sets, default:2]"
				<< std::endl;
		return -1;
	}
	input.open(argv[1], std::ios::in | std::ios::binary);
	if (!input.good()) {
		std::cerr << "Failed to open input file" << std::endl;
		return -2;
	}
	mapping.open("JIS0208.TXT", std::ios::in);
	if (!mapping.good()) {
		std::cerr
				<< "Failed to open mapping file (JIS0208.TXT). You can download it here:\nftp://ftp.unicode.org/Public/MAPPINGS/OBSOLETE/EASTASIA/JIS/JIS0208.TXT"
				<< std::endl;
		return -3;
	}
	output_cctable.open(argv[3], std::ios::out | std::ios::binary);
	if (!output_cctable.good()) {
		std::cerr << "Failed to create cc-table file" << std::endl;
		return -4;
	}
	if (argc > 4) {
		sheets_per_set = atoi(argv[4]);
	}
	if (argc > 5) {
		embedded_sets = atoi(argv[5]);
	}
	std::string line;
	while (!mapping.eof()) {
		std::getline(mapping, line);
		if (!line.size() || line[0] == '#') {
			continue;
		}
		int mapping_shift_jis;
		int mapping_jis;
		int mapping_unicode;
		sscanf(line.c_str(), "%x %x %x %x", &mapping_shift_jis, &mapping_jis,
				&mapping_unicode);
//		std::cout << line << std::endl;

		char utf8_builder[8] = { 0 };
//		std::cout << "Read: " << std::hex << mapping_unicode << std::endl;
		if (mapping_unicode > 0xffff) {
			std::cerr
					<< "Code point out of range. Currently only U+0x0000 - U+0xffff implemented. Got: U+0x"
					<< std::hex << mapping_unicode << std::endl;
			return -5;
		}
		if (mapping_unicode <= 0x7f) {
			utf8_builder[0] = mapping_unicode;
		} else if (mapping_unicode >= 0x80 && mapping_unicode <= 0x7ff) {
			utf8_builder[0] = ((mapping_unicode >> 6) & 0x1f) | 0xc0;
			utf8_builder[1] = (mapping_unicode & 0x3f) | 0x80;
		} else {
			utf8_builder[0] = (mapping_unicode >> 12) | 0xe0;
			utf8_builder[1] = ((mapping_unicode >> 6) & 0x3f) | 0x80;
			utf8_builder[2] = (mapping_unicode & 0x3f) | 0x80;
		}
		mapping_to_utf8[mapping_jis] = std::string(utf8_builder);
	}
	std::cout << "Sheets per set: " << sheets_per_set << std::endl;
	std::cout << "Embedded sets: " << embedded_sets << std::endl;
// Skip a header
	input.ignore(576);
	char output_filename[1024];
	int set_no = 0;
	int merged_set_no = 0;
	int sheet_no = 0;
	int previous_serial = -1;
	while (!input.eof()) {
		char tmp_buffer[3] = { 0 };
		unsigned short serial;
		unsigned short jis_code;
		char jis_reading[4];
		input.read((char *) tmp_buffer, 2);
		serial = (tmp_buffer[0] << 8) + tmp_buffer[1];
		input.read((char *) tmp_buffer, 2);
		jis_code = (tmp_buffer[0] << 8) + tmp_buffer[1];
		input.read((char *) &jis_reading, 4);
		if (previous_serial != serial) {
			// New sheet
			sheet_no++;
		}
		if (sheet_no == (sheets_per_set + 1) || previous_serial == -1) {
			if ((set_no % embedded_sets) == 0) {
				merged_set_no++;
				if (previous_serial != -1) {
					std::cout << "Closing previous file." << std::endl;
					output.close();
				}
				snprintf(output_filename, 1024, "%s.%03u.img", argv[2],
						merged_set_no);
				output.open(output_filename, std::ios::out | std::ios::binary);
				std::cout << "Opening new file: " << output_filename
						<< std::endl;
				if (!output.good()) {
					std::cerr << "Failed to create output file: "
							<< output_filename << std::endl;
					return -3;
				}
			}
			sheet_no = 1;
			set_no++;
		}
		if (merged_set_no == 1) {
			codes.push_back(jis_code);
		}
		output.write((char *) buffer_zero_before, 512);
		for (int row = 0; row < 63; row++) {
			input.read(buffer + 4, 8);
			output.write((char *) buffer, 16);
		}
		output.write((char *) buffer_zero_after, 528);
		input.ignore(64);
		previous_serial = serial;
	}
	output.close();
	char cctable_buffer[2048];
	for (std::vector<unsigned short>::iterator it = codes.begin();
			it != codes.end(); it++) {
		unsigned short code = *it;
		std::map<unsigned short, std::string>::iterator utf8_it =
				mapping_to_utf8.find(code);
		int size;
		if (utf8_it == mapping_to_utf8.end()) {
			std::cerr
					<< "Character not found in JIS -> Unicode conversion table: "
					<< code << std::endl;
			size = snprintf(cctable_buffer, 2048, "?\t?\tW\tCM\t-\tS\t-\n");
		} else {
			size = snprintf(cctable_buffer, 2048, "%s\t%s\tW\tCM\t-\tS\t-\n",
					utf8_it->second.c_str(), utf8_it->second.c_str());
		}
		output_cctable.write((char*) cctable_buffer, size);
	}
	output_cctable.close();
	return 0;
}
