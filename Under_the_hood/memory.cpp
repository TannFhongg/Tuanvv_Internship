/* 
Mỗi chương trình không trực tiếp làm việc với địa chỉ RAM thật. 
Nó nhìn thấy một không gian địa chỉ ảo riêng. 
Khi chương trình đọc hoặc ghi một địa chỉ,
phần cứng MMU dịch địa chỉ đó sang địa chỉ vật lý thông qua Page Table do hệ điều hành thiết lập. 
Paging chia bộ nhớ thành các page cố định và ánh xạ chúng tới các frame trong RAM. 
TLB lưu tạm các phép dịch thường dùng để tăng tốc.
Nếu page chưa có trong RAM hoặc chương trình không có quyền truy cập, MMU tạo page fault để kernel xử lý. 
Segmentation là một mô hình khác, chia bộ nhớ theo các vùng logic như code, data và stack;
trên hệ thống hiện đại Paging thường đóng vai trò chính.
*/

