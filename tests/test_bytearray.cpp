//
// Created by zavier on 2021/12/11.
//

#include "acid/byte_array.h"
#include "acid/log.h"
using namespace acid;
static Logger::ptr g_logger = ACID_LOG_ROOT();

void test1(){
    std::string str = "<2021-12-11 19:57:05	[ INFO	]	[root]	32507	main	0		tests/test_bytearray.cpp:20	1956297539--1956297539";
    std::vector<int> vec;
    ByteArray byteArray(1);
    for(int i = 0; i < 100 ; ++i){
        //vec.push_back(rand());
        //byteArray.writeFint32(vec[i]);
        byteArray.writeStringVint(str);
    }
    byteArray.setPosition(0);
    for(int i = 0; i < 100 ; ++i){
        //int a = byteArray.readFint32();
        std::string tmp = byteArray.readStringVint();
        ACID_LOG_INFO(g_logger) << tmp;
    }

}
class Student{
public:
    Student(){};
    Student(bool g, short age, int id, std::string name):m_isMale(g), m_age(age), m_stuId(id), m_name(name){}
    std::string toString(){
        std::stringstream ss;
        ss<< "[" <<(m_isMale? "male" : "female" ) << "," << m_age << "," << m_stuId << "," << m_name << "]";
        return ss.str();
    }
    void readFromByeArray(ByteArray& byteArray){
        m_isMale = byteArray.readFint8();
        m_age = byteArray.readFint16();
        m_stuId = byteArray.readFint32();
        m_name = byteArray.readStringF16();
    }
    void writeToByeArray(ByteArray& byteArray){
        byteArray.writeFint8(m_isMale);
        byteArray.writeFint16(m_age);
        byteArray.writeFint32(m_stuId);
        byteArray.writeStringF16(m_name);
    }
private:
    bool m_isMale = false;
    short m_age = 0;
    int m_stuId = 0;
    std::string m_name;
};

//序列化、反序列化
void test_2(){
    Student student(true, 20, 201912900, "Zavier");
    ByteArray byteArray{};
    student.writeToByeArray(byteArray);
    byteArray.setPosition(0);
    byteArray.writeToFile("Student.dat");

    ByteArray b;
    b.readFromFile("Student.dat");
    b.setPosition(0);
    Student s;
    s.readFromByeArray(b);

    ACID_LOG_INFO(g_logger) << s.toString();
}
void test3() {
    int n;
    ByteArray byteArray{};
    byteArray.writeInt32(128);
    byteArray.writeInt32(1);
    byteArray.setPosition(0);
    ACID_LOG_INFO(g_logger) << byteArray.toHexString();
    n = byteArray.readInt32();
    ACID_LOG_INFO(g_logger) << n;
    ACID_LOG_INFO(g_logger) << byteArray.toHexString();
}
int main(){
    test3();
}