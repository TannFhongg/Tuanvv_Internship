/*( 
Tạo object const thật sự, dùng const_cast rồi thử sửa; chạy sanitizer để quan sát hành vi không an toàn. 
)*/

#include <iostream>

class User {
    public :
    int age = 20 ;
};

class User_2 { 
    public: 
    int age = 20; 
}; 
class User_3 { 
    public :
    int age = 20; 
};
int main() { 
    User user; 
    const User& readonly = user; // tham chieu const 
    User& edit = const_cast<User&>(readonly); 
    edit.age = 23; 


    User_2 user_2; 
    const User_2* ptr = &user_2; // tham chieu con tro 
    User_2* edit_2 = const_cast<User_2*>(ptr); 
    edit_2 -> age = 22; 
/* 
ket luan : 
chi nen dung khi object goc thuc ra khong const , nhung dang giu no qua const poiter. reference
*/

// khong dung no voi object duoc khai bao la const 
const User_3 user_3; 
User_3* edit_3 = const_cast<User_3*>(&user_3); 
edit_3 -> age = 19; // UB 

}