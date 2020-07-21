#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct Header{
    uint16_t SEQ_NUM;
    uint16_t ACK_NUM;
    uint16_t SYN;
    uint16_t FIN;
    uint16_t ACK;
    uint16_t data;
}Header;


void createHeader(uint16_t SEQ_NUM, uint16_t ACK_NUM, uint16_t SYN, uint16_t FIN, uint16_t ACK, uint16_t data, Header* header){
    header->SEQ_NUM = SEQ_NUM;
    header->ACK_NUM = ACK_NUM;
    header->SYN = SYN;
    header->FIN = FIN;
    header->ACK = ACK;
    header->data = data; //size of data being sent
}

int hasSYN(Header* header){
    return header->SYN;
}

int hasACK(Header* header){
    return header->ACK;
}

int hasSYN_ACK(Header* header){
    return (header->SYN && header->ACK);
}

int hasFIN(Header* header){
    return header->FIN;
}

int hasFIN_ACK(Header* header){
    return (header->FIN && header->ACK);
}

int hasData(Header* header){
    return (header->data);
}




void printPacket(Header* header, char* msg){

    char buf[100];
    sprintf(buf,"%hu %hu ", header->SEQ_NUM, header->ACK_NUM);
    if(header->SYN){
        char buf1[5];
        sprintf(buf1, "SYN ");
        strcat(buf, buf1);
    }
    if(header->FIN){
        char buf2[5];
        sprintf(buf2, "FIN ");
        strcat(buf, buf2);
    }
    if(header->ACK){
        char buf3[5];
        sprintf(buf3, "ACK ");
        strcat(buf, buf3);
    }

    printf("%s %s\n", msg, buf);
}

void printDUP_ACK(Header* header, char* msg){

    char buf[100];
    sprintf(buf,"%hu %hu ", header->SEQ_NUM, header->ACK_NUM);
    printf("%s %sDUP_ACK\n", msg, buf);
}

void htonHeader(Header* header){
    header->SEQ_NUM = htons(header->SEQ_NUM);
    header->ACK_NUM = htons(header->ACK_NUM);
    header->SYN = htons(header->SYN);
    header->FIN = htons(header->FIN);
    header->ACK = htons(header->ACK);
    header->data = htons(header->data);

}

void ntohHeader(Header* header){
    header->SEQ_NUM = ntohs(header->SEQ_NUM);
    header->ACK_NUM = ntohs(header->ACK_NUM);
    header->SYN = ntohs(header->SYN);
    header->FIN = ntohs(header->FIN);
    header->ACK = ntohs(header->ACK);
    header->data = ntohs(header->data);
}

void bufferHeader(Header* header, uint16_t* buf){
//sprintf(buf,"%hu%hu%hu%hu%hu%hu", header->SEQ_NUM, header->ACK_NUM, header->SYN, header->FIN, header->ACK, header->pad);
    buf[0] = header->SEQ_NUM;
    buf[1] = header->ACK_NUM;
    buf[2] = header->SYN;
    buf[3] = header->FIN;
    buf[4] = header->ACK;
    buf[5] = header->data;
}


