/*!
 *   @File   FileTransfer.hpp
 *   @Author Michael Travaglione
 *   @Date   20 Sept 2010
 *   @Brief  Common data storage object needed by both server and client applications
 */
#pragma once

#include "SocketLibrary.hpp"   // network message types
#include <vector>

const unsigned MAX_FILENAME_LENGTH = 100;
const unsigned MAX_IP_LENGTH = 20;
const unsigned MAX_FILES = 50;

struct FileInfo 
{
    char fileName_[MAX_FILENAME_LENGTH];
};

typedef std::vector<FileInfo> FileInfoCont;

// Message types

struct MsgJoin
{
    struct Data 
    { 
        u32 fileCount_;
        FileInfo files_[MAX_FILES];
        SocketAddress udpAddr_;
    };

    Data data_;
    MsgType type_;
};

// struct MsgQuit
// {
//     MsgQuit(void) : type_(NetworkMessage::QUIT) { }
//     struct Data { };
// 
//     Data data_;
//     MsgType type_;
// };
// 
// struct MsgTransfer
// {
//     MsgTransfer(void) : type_(NetworkMessage::TRANSFER) { }
//     struct Data { };
// 
//     Data data_;
//     MsgType type_;
// };
// 
// struct MsgInformSender
// {
//     MsgInformSender(void) : type_(NetworkMessage::INFORM_SENDER) { }
//     struct Data { };
// 
//     Data data_;
//     MsgType type_;
// };
// 
// struct MsgInformReceiver
// {
//     MsgInformReceiver(void) : type_(NetworkMessage::INFORM_RECEIVER) { }
//     struct Data { };
// 
//     Data data_;
//     MsgType type_;
// };