var=$1
g++ -std=c++11 $@ -llibgo -lpthread -Wl,--no-as-needed -ldl -g -o ${var}"out"

