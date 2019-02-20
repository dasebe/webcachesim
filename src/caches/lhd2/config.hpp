#pragma once

namespace misc {

  struct ConfigReader {
      ConfigReader(const libconfig::Setting& _setting) : setting(_setting) {}

      const libconfig::Setting& setting;

      template<typename T>
      T search(const libconfig::Setting& root, std::string key) const {
          auto split = key.find('.');
          if (split == std::string::npos) {
              return root[key.c_str()];
          } else {
              return search<T>(root[key.substr(0,split).c_str()], key.substr(split+1, std::string::npos));
          }
      }

      bool exists(const libconfig::Setting& root, std::string key) const {
        auto split = key.find('.');
        if (split == std::string::npos) {
            return root.exists(key);
        } else {
            return exists(root[key.substr(0,split).c_str()], key.substr(split+1, std::string::npos));
        }
      }

      bool exists(std::string key) const {
        return exists(setting, key);
      }
      
      template<typename T>
      T read(std::string key) const {
          try {
              return search<T>(setting, key);
          } catch (const libconfig::SettingTypeException &tex) {
              std::cerr << "Type mismatched: " << key << std::endl;
              exit(-1);
          } catch (const libconfig::SettingNotFoundException &nfex) {
              std::cerr << "Setting not found: " << key << std::endl;
              exit(-1);
          }
      }

      template<typename T>
      T read(std::string key, T _default) const {
          try {
              return search<T>(setting, key);
          } catch (const libconfig::SettingTypeException &tex) {
              std::cerr << "Type mismatched: " << key << std::endl;
              exit(-1);
          } catch (const libconfig::SettingNotFoundException &nfex) {
              std::cerr << "Setting not found: " << key << ", using default value: " << _default << std::endl;
              return _default;
          }
      }
  };

}
