// Tuan tu hoa header

/*
Bạn cần tự implement

Bây giờ thực hiện phần đầu của Task 3.2B:

Tạo protocolcodec.h.
Khai báo một free function nhận ProtocolHeader và trả QByteArray.
Tạo protocolcodec.cpp.
Cấu hình QDataStream:
Big Endian.
Version cố định Qt 6.0.
Ghi từng field đúng thứ tự.
Chuyển MessageType tường minh thành quint16.
Kiểm tra trạng thái stream.
Nếu ghi thất bại, trả QByteArray rỗng.
Thêm protocolcodec.cpp vào target ProtocolCommon.
Build Debug và Release.
*/
#pragma once 
#include "protocolheader.h"
#include <QByteArray> 

namespace MiniCloud::Protocol {
    QByteArray serializeHeader(const ProtocolHeader &header, QByteArray &outData);
}
