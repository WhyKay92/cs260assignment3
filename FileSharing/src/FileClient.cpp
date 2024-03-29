/*!
 *   @File   FileClient.cpp
 *   @Author Michael Travaglione
 *   @Date   11/11/2010
 *   @Brief  Implementation of client for file transfer assignment.
 */

#include <algorithm>           // std::find
#include <iostream>

#include "File.hpp"            // File collector
#include "shared.hpp"
#include "SocketInterface.hpp"
#include "SocketLibrary.hpp"
#include "IOThread.hpp"

#include "FileClient.hpp"

void FileClient::Init( void )
{
    // initialize winsock
    try
    {
        StartWinSock();
    }
    catch(WSErr& e)
    {
        throw e;
    }

     // load config.ini file containing port and server ip
    config_.LoadConfigFile();

    GetLocalIP(localIP_);

     // set client port
    clientSock_.SetPortNumber(0);
    clientSock_.SetIP(localIP_);
    
    peerSock_.SetIP(localIP_);
    peerSock_.SetPortNumber(config_.udpPort_);

     // set udp port
    updAddr_.SetIP(localIP_);
    updAddr_.SetPortNumber(config_.udpPort_);

     // initialize socket used to connect to the server
    try
    {
        clientSock_.Init();
    }
    catch(iSocket::SockErr& e)
    {
        CloseWinSock();
        throw e;
    }

    try
    {
        peerSock_.SetIP(localIP_);
        peerSock_.SetPortNumber( config_.udpPort_ );

        peerSock_.Init();   
    }
    catch( iSocket::SockErr& e )
    {
        clientSock_.Close();
        CloseWinSock();
        e.Print();
    }


     // specify port and ip of remote connection, which will be the server
    remoteAddr_.SetIP(config_.ip_);
    remoteAddr_.SetPortNumber(config_.serverPort_);

    m_update_count = 10;
    requestFiles_ = false;
    m_show_prog = false;
}

///////////////////////////////////////////

void FileClient::ConnectToServer( void )
{
    try
    {
         // continue to try to connect to the server until the user quits
        connectedToServer_ = false;
        while(!connectedToServer_)
        {
            if(clientSock_.Connect(remoteAddr_))
            {
                IOObject::console.Wake();
                IOObject::console.Print("Connnection to Server established...\n Port: %d\n Server IP: %s\n",
                                         config_.serverPort_, config_.ip_.c_str());
                IOObject::console.Print("\nType '/help' for a list of available commands.\n\n");
                IOObject::console.Prompt();
                connectedToServer_ = true;
            }
            else
            {
                if(PromptYesNo("Unable to establish connection to server. Would you like to retry?"))
                    continue;
                else
                    break;

            }
        }
    }
    catch(iSocket::SockErr e)
    {
        throw e;
    }
}

///////////////////////////////////////////

void FileClient::SendFileList( void )
{
    FileCollector collector;
    std::string temp = config_.sendPath_ + "*.*";
    collector.SetDirectory(temp.c_str());
    collector.CollectFiles();

    MsgJoin join;
    FileName fInfo;
    unsigned fCount = 0;
    for(FileCollector::iterator it = collector.begin(); it != collector.end(); ++it)
    {
        strcpy(fInfo.fileName_, (*it).c_str());
        join.data_.files_[fCount] = fInfo;

        ++fCount;
        if(fCount >= MAX_FILES)
            break;
    }

    memcpy(files_, join.data_.files_, sizeof(join.data_.files_));

    join.type_ = NetworkMessage::JOIN;
    join.data_.fileCount_ = fCount;
    join.data_.udpAddr_.SetIP(localIP_);
    join.data_.udpAddr_.SetPortNumber(config_.udpPort_);

    NetworkMessage netMsg;
    netMsg << join;
    clientSock_.Send(netMsg);
}

///////////////////////////////////////////

void FileClient::Run( void )
{
    run_ = true;
    while(run_)
    {
        try
        {
            NetworkMessage netMessage;
            if(connectedToServer_)
            {
                if(clientSock_.Receive(netMessage))
                    ProcMessage(netMessage);
            }

            if(peerSock_.Receive(netMessage))
                ProcMessage(netMessage);

            peerSock_.Resend();

            std::string input;
            if(IOObject::console.GetMessages(input))
                ProcInput(input);

            UpdateTransfers();
        }
        catch( iSocket::SockErr e )
        {
            if(e.eCode_ == WSAECONNRESET)
            {
                IOObject::console.StartPrint();
                IOObject::console.Print("===========================\n");
                IOObject::console.Print("Connection to server lost.\n");
                IOObject::console.Print("===========================\n");
                IOObject::console.EndPrint();
                connectedToServer_ = false;
            }
        }
    }
}

///////////////////////////////////////////

void FileClient::Close(void)
{
    IOObject::console.Print("\nExiting server...\n\n");
    clientSock_.Close();
}

///////////////////////////////////////////

void FileClient::ProcInput( std::string& input )
{

    if(input == "/show")
    {
        if(connectedToServer_)
        {
            NetworkMessage request;
            request.type_ = NetworkMessage::REQ_FILES;
            requestFiles_ = true;

            clientSock_.Send(request);
        }
        else
        {
            IOObject::console.Print("\nConnection to server lost - no additionl transfers possible.\n\n");
            IOObject::console.Prompt();
        }
    }
    else if(input == "/help")
    {
        IOObject::console.StartPrint();

        IOObject::console.Print("\n/get [filename]  Request to transfer a file. Myst be a valid file on the server.");
        IOObject::console.Print("\n/help            Displays list of all valid commands and descriptions.");
        IOObject::console.Print("\n/quit            Exit server session.");
        IOObject::console.Print("\n/show            Request an updated list of files currently available from the server.\n\n");

        IOObject::console.EndPrint();
        IOObject::console.Prompt();
    }
    else if(input == "/quit")
    {
        NetworkMessage quitMsg;
        quitMsg.type_ = NetworkMessage::QUIT;
        clientSock_.Send(quitMsg);
        connectedToServer_ = false;
    }
    else if(input == "/prog")
    {
        m_show_prog = true;
        IOObject::console.Prompt();
    }
    else
    {
        if(!connectedToServer_)
        {
            IOObject::console.Print("\nConnection to server lost - no additionl transfers possible.\n\n");
            IOObject::console.Prompt();
            return;
        }

        std::string subString = input.substr(0, 5);
        if(subString == "/get ")
        {
            subString = input.substr(5);

            bool found = false;
            for(size_t i = 0; i < MAX_FILES; ++i)
            {
                if(!strcmp(files_[i].fileName_, subString.c_str()))
                    found = true;
            }

             if(found)
             {
                 MsgGet get;
                 strcpy(get.data_.name_.fileName_, subString.c_str());
                 get.type_ = NetworkMessage::GET;
                 get.data_.recvAddr_ = updAddr_;

                 NetworkMessage netMsg;
                 netMsg << get;
                 clientSock_.Send(netMsg);
             }
             else
                 IOObject::console.Print("\nInvalid filename requested. Type '/show' for an updated list of files.\n\n");

             IOObject::console.Prompt();
        }
        else
        {
            IOObject::console.Print("\nUnknown command - type '/help' for valid commands.\n\n");
            IOObject::console.Prompt();
        }
    }
}

///////////////////////////////////////////

void FileClient::ProcMessage( NetworkMessage& msg )
{
    switch(msg.type_)
    {
    case NetworkMessage::SERVER_FILES:
        {            
            IOObject::console.StartPrint();

            if(!requestFiles_)
                IOObject::console.Print("\nAvailable files on server have changed...\n\n");

            IOObject::console.Print("\nAvailable files on server: \n");
            IOObject::console.Print("========================== \n");
            MsgServerFiles fileMsg;
            msg >> fileMsg;

            unsigned numFiles = fileMsg.data_.fileCount_;
            for(unsigned i = 0; i < numFiles; ++i)
                IOObject::console.Print("%s\n", fileMsg.data_.files_[i].fileName_);

            memcpy(files_, fileMsg.data_.files_, sizeof(fileMsg.data_.files_));


            IOObject::console.Print("\n");
            IOObject::console.EndPrint();
            IOObject::console.Prompt();

            requestFiles_ = false;
            break;
        }
    case NetworkMessage::INFORM_SENDER:
        {
            MsgInformSender msgInform;
            msg >> msgInform;

            SplitPair tPear;

            tPear.first = msgInform.data_.recipient_;

            tPear.second.SetDirectory( config_.sendPath_ );
            tPear.second.SetFilename( msgInform.data_.fileName_ );
            tPear.second.SetChunkSize( DEFAULT_CHUNK_SIZE );

            tPear.second.InitChunking();
            if( tPear.second.IsGood() )
                outgoingTransfers_[ msgInform.data_.transferID_ ] = tPear;


            break;
        }
    case NetworkMessage::INFORM_RECEIVER:
        {
            MsgInformReceiver msgInform;
            msg >> msgInform;

            FileJoiner &joiner = incomingTransfers_[ msgInform.data_.transferID_ ];
            
            joiner.SetDirectory( config_.recvPath_ );
            joiner.SetFilename( msgInform.data_.fileName_ );
            joiner.SetChunkSize( DEFAULT_CHUNK_SIZE );

            peerSock_.AcceptFrom( msgInform.data_.sender_ );
            break;
        }
    case NetworkMessage::TRANSFER:
        {
            MsgTransfer transMsg;
            msg >> transMsg;

            JoinerMap::iterator it = incomingTransfers_.find( transMsg.data_.transferID_ );

            if( it != incomingTransfers_.end() )
            {
                FileJoiner &joiner = it->second;
                joiner.PutChunk( transMsg.data_.chunk_ );

                if( joiner.IsFileComplete() )
                {   
                    joiner.Close();

                    DWORD start_time = joiner.GetStartTime();
                    DWORD curr_time = timeGetTime();

                    float time_diff = ( float( curr_time - start_time ) );

                    IOObject::console.Print( "Completed transfering %s in %f seconds\n", joiner.GetFilename().c_str(), time_diff / 1000.0f );

                    incomingTransfers_.erase( it );
                }
            }

            break;
        }
    default:
        {
            break;
        }
    }
}


void FileClient::UpdateTransfers( void )
{
    for( SplitterMap::iterator it = outgoingTransfers_.begin(); it != outgoingTransfers_.end(); )
    {
        SplitPair &tPair = it->second;

        iFileInfo::Chunk chunk;
        if( tPair.second.GetNextChunk( chunk ) )
        {
            MsgTransfer transMsg;
            transMsg.data_.chunk_ = chunk;
            transMsg.data_.transferID_ = it->first;

            NetworkMessage netMsg;

            netMsg << transMsg;
            netMsg.receiverAddress_ = tPair.first;

            peerSock_.Send( netMsg );
        }

        if( tPair.second.ChunkedAll() )
        {
            tPair.second.Close();
            it = outgoingTransfers_.erase( it );
        }
        else
        {
            ++it;
        }
    }

    if( m_show_prog )
    {
      for( JoinerMap::iterator it = incomingTransfers_.begin(); it != incomingTransfers_.end(); ++it )
      {
        u32 chunks = it->second.GetCurrChunkCount();
        u32 total_chunks = it->second.GetChunkCount();
        u32 remaining_chunks = total_chunks - chunks;

        DWORD start_time = it->second.GetStartTime();
        DWORD curr_time = timeGetTime();

        float time_diff = float( curr_time - start_time );

        float ratio = time_diff / float( chunks ) ;
        float time = ratio * float( remaining_chunks );

        IOObject::console.Print( "Acquired %u of %u chunks.  Estimated remaining time is %f\n", chunks, total_chunks, time / 1000.0f );
      }

      IOObject::console.Prompt();

      m_show_prog = false;
    }

/*
    u32 i = 0;
    SplitterMap::iterator it = outgoingTransfers_.begin();

    while( i < m_update_count )
    {
        if( outgoingTransfers_.size() <= 0 )
            return;

        if( m_curr_update >= outgoingTransfers_.size() )
        {
            m_curr_update = 0;
        }

        if( it == outgoingTransfers_.end() )
            it = outgoingTransfers_.begin();

        SplitPair &tPair = it->second;

        iFileInfo::Chunk chunk;
        if( tPair.second.GetNextChunk( chunk ) )
        {
            MsgTransfer transMsg;
            transMsg.data_.chunk_ = chunk;
            transMsg.data_.transferID_ = it->first;

            NetworkMessage netMsg;

            netMsg << transMsg;
            netMsg.receiverAddress_ = tPair.first;

            peerSock_.Send( netMsg );
        }

        if( tPair.second.ChunkedAll() )
        {
            tPair.second.Close();
            it = outgoingTransfers_.erase( it );
        }
        else
        {
            ++it;
        }

        ++i;
        ++m_curr_update;
    }
*/

}

