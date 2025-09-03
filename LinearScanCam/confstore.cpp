#include "confstore.h"
#include <iostream>

ConfStore::ConfStore(std::string file) : filename(file)
{
    std::cout << "Starting conf store... " << file <<  std::endl;
    loadSettings(file);
    std::cout << "json: "<< config.dump() << std::endl;
}


void ConfStore::loadSettings(std::string file){

    json c;
    c["version"] = "0.1";

    c["ccd"]["rgain"] = 0x38;
    c["ccd"]["ggain"] = 0x32;
    c["ccd"]["bgain"] = 0x38;
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

    c["img"]["pnggamma"] = 2.2;
    c["img"]["pngcompress"] = 0;

    c["img"]["widthscale"] = 0.5;


    config = c; // Base config

    std::ifstream f(file);
    if(!f.fail()){ // Write new file
        std::cout << "Found config file" << std::endl;
        json newConf;
        f >> newConf;
        config.update(newConf,true);
    }
    f.close();

    std::ofstream of(file);
    of << config; // Update file again
    of.close();

}


void ConfStore::saveSettings(std::string file){
    std::ofstream of(file);
    of << config; // Update file again
    of.close();
}
