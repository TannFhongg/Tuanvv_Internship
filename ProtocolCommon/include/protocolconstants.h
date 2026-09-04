

#pragma once
#include <QtGlobal>
namespace MiniCloud::Protocol
{
inline constexpr quint32 protocolMagic = 0x4D434C44; // MCLD
inline constexpr quint16 protocolVersion = 1;
inline constexpr quint32 protocolWireHeaderSize = 28; // bytes
inline constexpr quint32 protocolMaxControlPayloadSize = 1024u * 1024u;
inline constexpr quint32 protocolMaxFileChunkDataBytes = 64u * 1024u; // 65536 bytes
inline constexpr quint32 protocolDefaultFileChunkSize = protocolMaxFileChunkDataBytes;
inline constexpr quint32 protocolMaxFramePayloadSize = 1024u * 1024u;
}
