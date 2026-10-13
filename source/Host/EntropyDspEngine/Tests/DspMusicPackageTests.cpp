#include "DspPackage.h"
#include "TestSupport.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QCryptographicHash>
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);QTemporaryDir tmp;CHECK(tmp.isValid());auto root=std::filesystem::path(tmp.path().toStdWString());std::string error;
 std::filesystem::create_directories(root/"sdk/include/entropy");QFile file(QString::fromStdWString((root/"EntropyDspEngine.exe").wstring()));CHECK(file.open(QIODevice::WriteOnly));file.write("payload");file.close();
 auto hash=QString::fromLatin1(QCryptographicHash::hash("payload",QCryptographicHash::Sha256).toHex());
 std::filesystem::copy_file(std::filesystem::path(TEST_SDK_ROOT)/"StageBusContract.json",root/"sdk/StageBusContract.json");std::filesystem::copy_file(root/"EntropyDspEngine.exe",root/"sdk/include/entropy/stage_bus_v1.h");
 QJsonArray entries{QJsonObject{{"path","EntropyDspEngine.exe"},{"sha256",hash}},QJsonObject{{"path","sdk/include/entropy/stage_bus_v1.h"},{"sha256",hash}},QJsonObject{{"path","sdk/StageBusContract.json"},{"sha256","1428f9ce6d6ee5fb00ca320f9436dc831992edf04991d9c54596b9646e296ecb"}}};
 QJsonObject manifest{{"stage","DSP_STAGE2"},{"stagebus_contract_sha256","1428f9ce6d6ee5fb00ca320f9436dc831992edf04991d9c54596b9646e296ecb"},{"files",entries}};
 auto write=[&]{QFile m(QString::fromStdWString((root/"package-manifest.json").wstring()));return m.open(QIODevice::WriteOnly) && m.write(QJsonDocument(manifest).toJson())>0;};
 CHECK(write());CHECK(!verifyDspPackage(root,error)); // legacy-only package cannot describe this MUSIC binary
 std::filesystem::copy_file(std::filesystem::path(MUSIC_CONTRACT_ROOT)/"MusicStageBusContract.json",root/"sdk/MusicStageBusContract.json");
 QFile contract(QString::fromStdWString((root/"sdk/MusicStageBusContract.json").wstring()));CHECK(contract.open(QIODevice::ReadOnly));auto musicHash=QString::fromLatin1(QCryptographicHash::hash(contract.readAll(),QCryptographicHash::Sha256).toHex());contract.close();
 entries.append(QJsonObject{{"path","sdk/MusicStageBusContract.json"},{"sha256",musicHash}});manifest.insert("files",entries);manifest.insert("music_stagebus_contract_sha256",musicHash);manifest.insert("input_channels",2);manifest.insert("output_channels",3);
 CHECK(write());CHECK(verifyDspPackage(root,error));
 manifest.insert("output_channels",11);CHECK(write());CHECK(!verifyDspPackage(root,error));manifest.insert("output_channels",3);
 manifest.insert("music_stagebus_contract_sha256",QString(64,'0'));CHECK(write());CHECK(!verifyDspPackage(root,error));manifest.insert("music_stagebus_contract_sha256",musicHash);CHECK(write());
 CHECK(file.open(QIODevice::Append));file.write("changed");file.close();CHECK(!verifyDspPackage(root,error));
 std::cout<<"PASS MUSIC package requires 2/3 contract, refuses legacy-only metadata, changed hash/payload\n";
}
