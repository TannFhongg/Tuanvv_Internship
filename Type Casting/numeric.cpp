/*
Tạo ví dụ numeric narrowing (double sang int) và ghi rõ mất mát dữ liệu.
Hien tuong chuyen doi mot kieu du lieu co pham vi luu tru lon hon , do chin xac cao hon 
ve mot kieu du lieu co pham vi luu tru nho hon 

vi du : so thuc -> so nguyen double -> int
// mat phan thap phan  
long long -> int -> short, char 
// nguy co tran so 
sign -> unsigned 
// sai lech nghiem trong bu tru 2 
*/

# include <iostream> 
#include <limits>
#include <stdexcept>
int main() { 

double price = 123.456; 

int wholePrice = static_cast<int>(price);  
std::cout << wholePrice ; // 123


double value =  -8.910; 

int result = static_cast<int>(value); 
std::cout << result ; // -8 


// kiem tra pham vi dau truoc khi ep kieu 

double number = 3'000'000'000.0; 

if(number > std::numeric_limits<int>::max() || number < std::numeric_limits<int>::min()) {
    std::cout << "vuot pham vi gia tri " << std::endl; 
}

int convert = static_cast<int>(number); 


}