#include "confstore.h"
#include <iostream>
#include <filesystem>

ConfStore::ConfStore(std::string file) : filename(file)
{
    std::cout << "Starting conf store... " << file <<  std::endl;
    loadSettings(file);
    std::cout << "json: "<< config.dump() << std::endl;
}


void ConfStore::loadSettings(std::string file){

    json c;
    c["version"] = "0.1";

    c["ccd"]["rgain"] = 63;//0x38; //63
    c["ccd"]["ggain"] = 54;//0x32; //54
    c["ccd"]["bgain"] = 60;//0x38; //60
    c["ccd"]["rofs"] = 0x7E;
    c["ccd"]["gofs"] = 0x77;
    c["ccd"]["bofs"] = 0x7E;
    c["ccd"]["res"] = 3;

    c["imagepath"] = "/mnt/ssd/";
    c["imagepathfullres"] = "/mnt/ssd/img/";
    c["imagepathlowres"] = "/mnt/ssd/preview/";
    c["previewfile"] = "preview.png";
    c["camtunefile"] = "";
    c["mot"]["slidesg"] = 30;
    c["mot"]["slidespeed"] = 800;
    c["mot"]["slideaccel"] = 5;

    c["img"]["pnggamma"] = 1.0;
    c["img"]["exposurems"] = 0; // Line period, 0 = base exposure
    c["img"]["previewexp"] = false; // Preview with the exposure equivalent to the capture
    c["img"]["pngcompress"] = 0;

    c["img"]["widthscale"] = 0.5;


    config = c; // Base config

    std::ifstream f(file);
    if(!f.fail()){ // Write new file
        std::cout << "Found config file" << std::endl;
        try{
            json newConf = json::parse(f);
            config.update(newConf,true);
        }catch(const json::exception& e){
            // Keep the broken file for inspection and continue with defaults
            std::cout << "Invalid config file, using defaults: " << e.what() << std::endl;
            config = c;
            f.close();
            std::error_code ec;
            std::filesystem::rename(file,file + ".bak",ec);
        }
    }
    f.close();

    saveSettings(file); // Update file again
}


void ConfStore::saveSettings(std::string file){
    // Write to a temp file and rename so a crash or power loss can't leave a truncated config
    const std::string tmpfile = file + ".tmp";
    std::ofstream of(tmpfile);
    of << config.dump(1);
    of.close();
    if(of.fail()){
        std::cout << "Failed to write config " << tmpfile << std::endl;
        return;
    }
    std::error_code ec;
    std::filesystem::rename(tmpfile,file,ec);
    if(ec){
        std::cout << "Failed to save config " << file << ": " << ec.message() << std::endl;
    }
}
