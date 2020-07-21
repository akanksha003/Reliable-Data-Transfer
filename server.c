#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "Header.h"
#include "Timer.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>


// *** CONSTANTS ***
#define MAX_SEQ_NUM 25601
#define MAX_DATA_SIZE 512
#define PACKET_SIZE 524
#define HEADER_SIZE 12
#define MAX_WINDOW_SIZE 10

int SENT_FIN = 0;
int connections = 0;
char name[20] = "";


struct timeval timeOut;
struct timeval cur_time;

int main(int argc, char* argv[])
{
  if(argc != 2){
  perror("Please Enter Port Number\n");
  exit(1);
  }
  //uint16_t port = (uint16_t)atoi(argv[1]);
  uint16_t port = 0;
  port = (uint16_t)atoi(argv[1]);

  uint16_t SEQ_NUM = rand();
  uint16_t EXPECTED_SEQ_NUM;
  srand(time(NULL));

 // *** Initialize socket for listening ***
 int sockfd;
 if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
   perror("socket");
   exit(1);
 }

 // *** Initialize local listening socket address ***
 struct sockaddr_in my_addr;
 memset(&my_addr, 0, sizeof(my_addr));
 my_addr.sin_family = AF_INET;
 my_addr.sin_port = htons(port);
 my_addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY allows to connect to any one of the host’s IP address

 // *** Socket Bind ***
 if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
   perror("bind");
   exit(1);
 }


 struct sockaddr_in client_addr; // client address
 unsigned int sin_size;
 uint16_t recv_buf[524];
 int recv_buf_size;
 int RCV_FIN_ACK = 0;

 while (1) { // main accept() loop
   sin_size = sizeof(struct sockaddr_in);
   //fcntl(client_fd, F_SETFL, O_NONBLOCK);

   connections++;
   memset(name, 0, 20);
   sprintf(name,"%d.file", connections);

   //printf("--------------------------------\n");
   FILE* fp = NULL;

   memset(recv_buf, 0, 524);
   Header s_header;
   uint16_t ACK_NUM;
   uint16_t buf[12];
   SENT_FIN = 0;
   uint16_t data = 0;
   while(1){
       if ((recv_buf_size = recvfrom(sockfd, recv_buf, 524, MSG_DONTWAIT,(struct sockaddr *)&client_addr, &sin_size)) > 0){

            Header r_header;
            createHeader(recv_buf[0],recv_buf[1],recv_buf[2],recv_buf[3],recv_buf[4], recv_buf[5], &r_header);
            ntohHeader(&r_header);
            if(hasData(&r_header) && hasFIN(&r_header)){
                //fprintf(stderr, "This is not a header pkt\n");
                continue; //can't be a header pkt
            }
            if(hasSYN(&r_header) && hasFIN(&r_header)){
               //fprintf(stderr,"This is not a header pkt\n");
                continue; //can't be a header pkt
            }
            if(hasData(&r_header) && hasSYN(&r_header)){
                //fprintf(stderr,"This is not a header pkt\n");
                continue; //can't be a header pkt
            }

            if(hasData(&r_header)){
            //printf("This header has data\n");
            }
            printPacket(&r_header, "RECV");
            // *** SYN ACK ***


            if(hasSYN(&r_header)){
                SEQ_NUM = rand() % MAX_SEQ_NUM;
                ACK_NUM = (r_header.SEQ_NUM +1) % MAX_SEQ_NUM;
                EXPECTED_SEQ_NUM = ACK_NUM;

                // **** SEND SYN ACK ****
                createHeader(SEQ_NUM, ACK_NUM, 1, 0, 1, 0, &s_header);
                printPacket(&s_header, "SEND");
                htonHeader(&s_header);
                //uint16_t buf[12];
                memset(buf, '\0', 12);
                bufferHeader(&s_header, buf);
                sendto(sockfd, buf, 12, 0, (struct sockaddr *)&client_addr, sin_size);
                continue;
            }

            //*** pure data packet ***
            else if(!hasSYN(&r_header) && !hasFIN(&r_header) && !hasACK(&r_header) && hasData(&r_header)){
                   //printf("pure data pkt arrived\n");
                   if(r_header.SEQ_NUM != EXPECTED_SEQ_NUM){
                        createHeader(SEQ_NUM, ACK_NUM, 0, 0, 1, 0, &s_header);
                        printDUP_ACK(&s_header, "SEND");
                        htonHeader(&s_header);
                        memset(buf, '\0', 12);
                        bufferHeader(&s_header, buf);
                        sendto(sockfd, buf, 12, 0, (struct sockaddr *)&client_addr, sin_size);
                        continue;
                   }

                   //printf("im here\n");
                   while(1){
                       if((recv_buf_size = recvfrom(sockfd, recv_buf, 524, MSG_DONTWAIT,(struct sockaddr *)&client_addr, &sin_size)) > 0){
                            ACK_NUM = (r_header.SEQ_NUM + r_header.data) % MAX_SEQ_NUM;
                            EXPECTED_SEQ_NUM = ACK_NUM;
                            //printf("Size is %d\n", recv_buf_size);
                            //printf("problem after/while writing the file\n");
                            fwrite(recv_buf, 1, r_header.data, fp);

                            //data = 0;
                            createHeader(SEQ_NUM, ACK_NUM, 0, 0, 1, 0, &s_header);
                            printPacket(&s_header, "SEND");
                            htonHeader(&s_header);
                            //uint16_t buf[12];
                            memset(buf, '\0', 12);
                            bufferHeader(&s_header, buf);
                            sendto(sockfd, buf, 12, 0, (struct sockaddr *)&client_addr, sin_size);
                            break;
                       }
                   }
                   continue;

            }

            // *** Receiving ACK for SYNACK, with 1 payload ***
            // but ACK from client could be for FIN or SYNACK. if it is for FIN, close connection
            else if(hasACK(&r_header)){
                if(SENT_FIN){
                    RCV_FIN_ACK = 1;
                    //close connection
                    //printf("leaving this connection\n");
                    fclose(fp);
                    break;
                }

                //Header s_header; //This is an ACK for data sent by client
                 data = r_header.data;
                //printf("%d\n", data);


                 if(data){
                       if(r_header.SEQ_NUM != EXPECTED_SEQ_NUM){
                            createHeader(SEQ_NUM, ACK_NUM, 0, 0, 1, 0, &s_header);
                            printDUP_ACK(&s_header,"SEND");
                            htonHeader(&s_header);
                            memset(buf, '\0', 12);
                            bufferHeader(&s_header, buf);
                            sendto(sockfd, buf, 12, 0, (struct sockaddr *)&client_addr, sin_size);
                            continue;
                       }

                //data incoming
                    //printf("waiting for the data. the received buf size was %d", recv_buf_size);
                       while(1){
                           if((recv_buf_size = recvfrom(sockfd, recv_buf, 524, MSG_DONTWAIT,(struct sockaddr *)&client_addr, &sin_size)) > 0){
                            ACK_NUM = (r_header.SEQ_NUM + data) % MAX_SEQ_NUM;
                            SEQ_NUM = r_header.ACK_NUM % MAX_SEQ_NUM;
                            EXPECTED_SEQ_NUM = ACK_NUM;
                            //printf("Size is %d\n", recv_buf_size);
                            if((fp = fopen(name, "a+" )) == NULL ){
                                   perror("Error Creating/opening file\n");
                                   exit(2);
                            }

                            fwrite(recv_buf, 1, r_header.data, fp);

                            data = 0;
                            createHeader(SEQ_NUM, ACK_NUM, 0, 0, 1, 0, &s_header);
                            printPacket(&s_header, "SEND");
                            htonHeader(&s_header);
                            //uint16_t buf[12];
                            memset(buf, '\0', 12);
                            bufferHeader(&s_header, buf);
                            sendto(sockfd, buf, 12, 0, (struct sockaddr *)&client_addr, sin_size);
                            break;
                        }
                    }
                    continue;

                }
                else{
                //printf("Wheres the data bich\n");
                continue;
                }

            }


            // *** RECV FIN from client. Respond with ACK and SEND OWN FIN ***
            else if(hasFIN(&r_header) && !hasData(&r_header)){
                //recvd FIN, so send ACK followed by FIN
                Header ACK_pkt;
                ACK_NUM = (r_header.SEQ_NUM + 1) % MAX_SEQ_NUM;
                createHeader(SEQ_NUM, ACK_NUM, 0, 0, 1, 0, &ACK_pkt);
                printPacket(&ACK_pkt, "SEND");
                htonHeader(&ACK_pkt);
                memset(buf, '\0', 12);
                bufferHeader(&ACK_pkt, buf);
                sendto(sockfd, buf, 12, 0, (struct sockaddr *)&client_addr, sin_size);
                // *** FIN ***
                Header FIN_pkt;
                createHeader(SEQ_NUM, 0, 0, 1, 0, 0, &FIN_pkt);
                printPacket(&FIN_pkt, "SEND");
                htonHeader(&FIN_pkt);
                memset(buf, '\0', 12);
                bufferHeader(&FIN_pkt, buf);
                sendto(sockfd, buf, 12, 0, (struct sockaddr *)&client_addr, sin_size);
                setTimer(&timeOut);
                SENT_FIN = 1;
                continue;
             }

             else if(recv_buf_size < 0){
                   //printf("Nothing to read from client side\n");
                   //close(client_fd);
                   continue;
             }
             else{
                //printf("the fk is happenin\n");
                //exit(2);
                continue;
             }

       }
       if(SENT_FIN && isTimeOut(&cur_time, &timeOut)){
                printf("TIMEOUT %hu\n", SEQ_NUM);
                Header FIN_pkt;
                createHeader(SEQ_NUM, 0, 0, 1, 0, 0, &FIN_pkt);
                printPacket(&FIN_pkt, "RESEND");
                htonHeader(&FIN_pkt);
                memset(buf, '\0', 12);
                bufferHeader(&FIN_pkt, buf);
                sendto(sockfd, buf, 12, 0, (struct sockaddr *)&client_addr, sin_size);
                setTimer(&timeOut);
                SENT_FIN = 1;
                continue;
       }
    }
 }

}



