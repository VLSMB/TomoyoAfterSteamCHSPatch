#include "hook.h"
#include "define.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <system_error>
#include <cstdint>

bool SaveBytesToFile(const std::vector<BYTE>& data, const std::string& path) {
    namespace fs = std::filesystem;
    try {
        fs::path p(path);
        if (p.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(p.parent_path(), ec);
            if (ec) {
                return false;
            }
        }

        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            return false;
        }

        if (!data.empty()) {
            ofs.write(reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size()));
            if (!ofs) {
                return false;
            }
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

int main(void) {
	auto opt = PackFromTextFile();
	if (!opt) {
		std::cout << "Pack Files FAIL" << std::endl;
		return 1;
	}
    if (!SaveBytesToFile(opt->saveAsBinFile(), BIN_DATA_FILE)) {
        std::cout << "Save Pack File FAIL" << std::endl;
        return 2;
    }
	std::cout << "SUCCESS" << std::endl;
	return 0;
}