#pragma once

#include <Poco/Net/Socket.h>

#include "BufferWithOwnMemory.h"
#include "WriteBuffer.h"


namespace RK
{

/** Works with the ready Poco::Net::Socket. Blocking operations.
  */
class WriteBufferFromPocoSocket : public BufferWithOwnMemory<WriteBuffer>
{
protected:
    Poco::Net::Socket & socket;

    /** For error messages. It is necessary to receive this address in advance, because,
      *  for example, if the connection is broken, the address will not be received anymore
      *  (getpeername will return an error).
      */
    Poco::Net::SocketAddress peer_address;


    void nextImpl() override;

public:
    WriteBufferFromPocoSocket(Poco::Net::Socket & socket_, size_t buf_size = DEFAULT_BUFFER_SIZE);

    ~WriteBufferFromPocoSocket() override;
};

}
