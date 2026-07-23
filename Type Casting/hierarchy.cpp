/*
Runtime Type Infomation 
Viết hierarchy Shape -> Circle/Rectangle có destructor virtual; thử downcast đúng/sai bằng static_cast và dynamic_cast.
*/

#include <iostream>
#include <memory>
class Shape
{
public:
    virtual float area() = 0;
    virtual float perimeter() = 0;

    virtual ~Shape() = default;
};

class Rectangle : public Shape
{

private:
    float height_, width_;

public:
    Rectangle(float width, float height) : width_(width), height_(height) {}

    float area() override
    {
        return width_ * height_;
    }

    float perimeter() override
    {
        return 2 * (width_ + height_);
    }
};

class Circle : public Shape
{

private:
    float radius_;

public:
    Circle(float radius) : radius_(radius) {}

    float area() override
    {
        return 3.14 * radius_ * radius_;
    }

    float perimeter() override
    {
        return 2 * 3.14 * radius_;
    }
};

void display(Shape *shape)
{
    std::cout << "Area: " << shape->area() << std::endl;
    std::cout << "Perimeter: " << shape->perimeter() << std::endl;
}

int main()
{

    std::unique_ptr<Shape> shape = std::make_unique<Circle>(7.0);

    // dynamic cast
    if (auto *circle = dynamic_cast<Circle *>(shape.get())) //( Shape * )
    {
        std::cout << "this is dynamic cast " << std::endl;
        display(circle);
    }

    try {
        Circle& circle = dynamic_cast<Circle&>(*shape);
    }
    catch (const std::bad_cast&) {
        std::cout << "It not circle" <<  std::endl; 
    }

    // dynamic sai - > return nullptr
    if (auto *rectangle = dynamic_cast<Rectangle*>(shape.get()))
    {
        std::cout << "this is dynamic cast " << std::endl;
        display(rectangle);
    }
    else
    {
        std::cout << "Khong the ep Circle thanh Rectangle" << std::endl;
    }

    //static cast 

    Circle * circle = static_cast<Circle*>(shape.get()); 
    display(circle); 

    // static cast sai , ket qua la van ep duoc nhung ket qua la UB 
    Rectangle* rect = static_cast<Rectangle*>(shape.get()); 
    display(rect);  

    }