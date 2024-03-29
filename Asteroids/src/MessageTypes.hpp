/*!
 *   @File   FileTransfer.hpp
 *   @Author Michael Travaglione
 *   @Date   20 Sept 2010
 *   @Brief  Common data storage objects needed by both server and client applications
 */
#pragma once

#include "SocketLibrary.hpp"   // network message types

const unsigned MAX_USERNAME_LEN = 20;
const unsigned KEY_INFO_BUFF_LEN = 32;
const unsigned MAX_PLAYERS = 8;

enum KeyState { KEY_DOWN, KEY_TRIGGERED };

struct KeyInfo
{
    u32 key_;
    KeyState state_;
};

struct Username 
{
    Username(void) { memset(name_, 0, MAX_USERNAME_LEN); }
    char name_[MAX_USERNAME_LEN];

    bool operator ==( Username const &rhs )
    {
        u32 len1 = strlen( name_ );
        u32 len2 = strlen( rhs.name_ );

        if( len1 != len2 )
            return false;

        return ( strcmp( name_, rhs.name_ ) == 0 );
    }
};

struct ResultStatus
{
    Username name_;
    u32 score_;
};


// Message types

struct MsgJoin
{
    MsgJoin(void) : type_(NetworkMessage::JOIN) { }
    struct Data 
    { 
        Username username_;
    };

    Data data_;
    MsgType type_;
};

struct MsgInput 
{
    MsgInput(void) : type_(NetworkMessage::INPUT) { }
    struct Data
    {
        Data(void) : key_info_count_(0) { }
        Username username_;
        KeyInfo key_data_[KEY_INFO_BUFF_LEN];
        u32 key_info_count_;
    };

    Data data_;
    MsgType type_;
};

struct MsgPosUpdate
{
    MsgPosUpdate(void) : type_(NetworkMessage::POS_UPDATE) { }
    u32 const static buf_size_ = 3800;

    struct Data
    {
        char inst_data_[ buf_size_ ];
        u32 inst_count_;

        double time_left_;
        u32 round_;
    };

    Data data_;
    MsgType type_;
};

struct MsgOutcome
{
    MsgOutcome(void) : type_(NetworkMessage::OUTCOME) { }

    struct Data
    {
        ResultStatus status_data_[MAX_PLAYERS];
        u32 num_players_;
        bool final_;
    };

    Data data_;
    MsgType type_;
};
