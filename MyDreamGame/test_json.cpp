#include <iostream>
#include "project/externals/nlohmann/json.hpp"

int main() {
    nlohmann::json data_;
    try {
        std::cout << "contains: " << data_.contains("group") << std::endl;
        data_["group"]["key"] = 20.0f;
        std::cout << "assigned: " << data_["group"]["key"] << std::endl;
    } catch(const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
