#include "binary_encrypt_mgr.h"

#define MAX_HEADER_LENGTH 512

namespace binary_encrypt_mgr
{
	void binary_encrypt(const char* bi, const char* mdi,const char* bo, const char* mdo) {
		// process binary
		uint32_t crc_x32;
		uint32_t crc_x64;
		proc_binary(bi, bo, crc_x32, crc_x64);
		proc_metadata(mdi, mdo, crc_x32, crc_x64);
		cout << "[binary_encrypt_mgr] encrypt succ" << endl;
	}

	void proc_binary(const char* bi, const char* bo, uint32_t& crc_x32, uint32_t& crc_x64) {
		char* binary_data;
		size_t sz_bin = utils::read_file(bi, &binary_data);

		if (sz_bin == 0) {
			throw "[binary_encrypt_mgr] failed to open il2cppbinary file";
		}

		string platform = "Unknown";
		string architecture = "Unknown";

		if (((int*)binary_data)[0] == 0x905A4D) {// MZ
			platform = "WIN32";

			int pe_addr = 0;
			while (true) {
				if (*((int*)(binary_data + pe_addr)) == 0x4550) {// PE00
					break;
				}
				pe_addr += sizeof(int);
				if (pe_addr > sz_bin) {
					break;
				}
			}
			if (binary_data[pe_addr + 1] == 0x4C) {// PE..L
				architecture = "x86";
			}
			if (binary_data[pe_addr + 1] == 0x64) {// PE..d
				architecture = "x64";
			}
		}
		if (((int*)binary_data)[0] == 0x464C457F) {// ELF
			
			if (binary_data[4] == 1) {
				architecture = "armeabi-v7a";
			}
			if (binary_data[4] == 2) {
				architecture = "arm64-v8a";
			}
		}
		cout << "[binary_encrypt_mgr] il2cppbinary platfrom : "<<platform << endl;
		cout << "[binary_encrypt_mgr] il2cppbinary architecture : "<< architecture << endl;

		uint32_t crc_bin = encryption::crc32(binary_data, sz_bin);
		printf("[binary_encrypt_mgr] il2cppbinary %s %s crc32 is %08x\n", platform.c_str(), architecture.c_str(), crc_bin);

		// TODO: Classify x32 and x64
		crc_x32 = crc_bin;
		crc_x64 = crc_bin;

		cout << "[binary_encrypt_mgr] binary "<<platform <<" " << architecture << " encrypted" << endl;
	}

	void proc_metadata(const char* mdi, const char* mdo, uint32_t crc_x32, uint32_t crc_x64) {
		// process globalmetadata

		char* data_o;
		size_t s_o = utils::read_file(mdi, &data_o);

		if (s_o == 0) {
			throw "[binary_encrypt_mgr] failed to open il2cppbinary file";
		}

		// check sanity
		if (*((int*)data_o) != 0xFAB11BAF) {
			cout << "[binary_encrypt_mgr] invaild metadata file" << endl;
			if (utils::is_show_mb_err()) {
				MessageBox(NULL, L"��Ч��metadata�ļ�", L"����", MB_ICONERROR);
			}
			throw "[binary_encrypt_mgr] invaild metadata file";
		}

		// expend 1kb to save some important data
		char* data = (char*)malloc(s_o + 1024);
		memcpy(data, data_o, s_o);
		mt19937 rand = mt19937(std::random_device()());
		// Fill
		for (int i = 0; i < 1024; i++) {
			data[i + s_o] = rand();
		}

		// TODO : proc string first, otherwise we don't know the real positions
		//proc_strings(data, s_o);
		//proc_stringslit(data, s_o);


		// TODO : proc Header
		//vector<wstring> head_script;
		//utils::read_file_lines_w("./oz_scripts/gbmd_head.ozs", head_script);

		// TODO : This method is only for debug, write a txt reader
		for (int i = 1; i <= 29; i++) {
			swap_header_int32(data, i, 60 - i);
			cout << "[binary_encrypt_mgr] dbg:swap line " << i << " " << 60 - i << endl;
		}

		// generate head
		generate_ozmetadata_header(data, s_o, crc_x32, crc_x64);

		// all encrypt
		const char* key = "REPLACE_THIS_FOR_A_CUSTOM_KEY";
		// dont encrypt fronthdr now, so that we can get file length
		encryption::oz_encryption(data + sizeof(FrontHeader), s_o - sizeof(FrontHeader), key, strlen(key));
		// encrypt fronthdr now
		encryption::oz_encryption(data, sizeof(FrontHeader), key, strlen(key));

		utils::write_file(mdo, data, s_o + 1024);

		cout << "[binary_encrypt_mgr] global-metadata.dat file encrypted" << endl;
	}

	void swap_header_int32(char* data, int p1,int p2) {
		int* head = (int*)data;
		int h = head[p1-1];
		head[p1-1] = head[p2-1];
		head[p2-1] = h;
	}

	void generate_ozmetadata_header(char* data, size_t len_o, uint32_t crc_x32, uint32_t crc_x64) {
		// copy header
		//Start head = data + len_o;
		memcpy((data + len_o), data, MAX_HEADER_LENGTH);

		// encrypt header
		/*size_t hl;
		xxtea_encrypt(data, MAX_HEADER_LENGTH, "o&z_il2cpp_encryption", &hl);*/
		const char* header_key = "REPLACE_THIS_FOR_A_CUSTOM_KEY";
		encryption::oz_encryption((data + len_o), MAX_HEADER_LENGTH, header_key, strlen(header_key));
		/*for (int i = 0; i < MAX_HEADER_LENGTH; i++) {
			data[i + len_o] ^= header_key[i % keylen];
		}*/

		// create header
		FrontHeader* fhdr = (FrontHeader*)malloc(sizeof(FrontHeader));
		if (fhdr == NULL) {
			cout << "[binary_encrypt_mgr] mem error" << endl;
			exit(ERR_CODE_MEM);
		}

		fhdr->sanity = 8102084797857888879;

		fhdr->length = MAX_HEADER_LENGTH;
		fhdr->offset = len_o;

		fhdr->crc_x64 = crc_x64;
		fhdr->crc_x32 = crc_x32;
		
		mt19937 rand = mt19937(std::random_device()());
		for (int i = 0; i < 32; i++) {
			fhdr->key[i] = rand();
			fhdr->key[i] = 0;
		}

		// destroy original header
		for (int i = 0; i < 200; i++) {
			data[i] = rand();
			data[i] = 0;
		}

		// TODO : calc metadata's crc

		memcpy(data, fhdr, sizeof(FrontHeader));
	}

	void proc_strings(char* data, size_t len) {
		// int32_t sanity; 
		// int32_t version;
		// int32_t stringLiteralOffset; // string data for managed code
		// int32_t stringLiteralCount;
		// int32_t stringLiteralDataOffset;
		// int32_t stringLiteralDataCount;
		// int32_t stringOffset; // string data for metadata
		// int32_t stringCount;
		int* head = (int*)data;
		int stringsOffset = head[6];
		int stringsCount = head[7];
		cout << "[binary_encrypt_mgr] StringsOffset : " << stringsOffset <<endl;
		cout << "[binary_encrypt_mgr] StringsCount : " << stringsCount <<endl;

		int key = 114;

		for (int i = 0; i < stringsCount; i++) {
			char x = *(data + stringsOffset +i);
			if (x != 0 && x != key) {
				*(data + stringsOffset +i) ^= key;
			}
		}
	}

	void proc_stringslit(char* data, size_t len) {
		int* head = (int*)data;
		int stringLiteralOffset = head[2]; // string data for managed code
		int stringLiteralCount = head[3];
		int stringLiteralDataOffset = head[4];
		int stringLiteralDataCount = head[5];


	}
};