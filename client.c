#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Header.h"
#include "Timer.h"
#include <fcntl.h>

// ***HELPER FUNCTIONS ***
/* returns total # 512 byte pkts needed to transmit entire file <filename>*/
int numPkts(long file_size);
/* Reads to_read bytes from open file fp into buffer buf*/
long initPayload(FILE* fp, long file_size, int pktnum, int totalpkts, char* buf);


// *** CONSTANTS ***
#define MAX_SEQ_NUM 25601
#define MAX_DATA_SIZE 512
#define PACKET_SIZE 524
#define HEADER_SIZE 12
#define MAX_WINDOW_SIZE 10

// *** Timer Variables ***
struct timeval timeOut;
struct timeval cur_time;
uint16_t LAST_ACK = 0;
uint16_t LAST_NEXT_SEQ_NUM = 0;


int main(int argc, char* argv[])
{
  if(argc != 4){
      perror("Please enter all arguments in order: hostname port# filename\n");
      exit(1);
  }

  char hostname[20] = "\0";
  strcpy(hostname, argv[1]);

  uint16_t port = 0;
  port = (uint16_t)atoi(argv[2]);

  char filename[20] = "\0";
  strcpy(filename, argv[3]);
  FILE* fp = fopen(filename, "r");
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  rewind(fp);
  char* data = (char*)malloc((MAX_DATA_SIZE+1)*sizeof(char));
  memset(data, '\0', MAX_DATA_SIZE);
  int pktnum = 0;
  int totalpkts = 0;
  int pkts_sent = 0; //packets sent in curr window that are not acked
  totalpkts = numPkts(size);
  int pkts_resent = 0;
  uint16_t send_base = 0;


  uint16_t SEQ_NUM = rand();
  uint16_t NEXT_SEQ_NUM = 0;
  srand(time(NULL));
  int syn = 0;

 // *** Initialize Socket ***
 int sockfd;
 if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror("Cannot create socket");
    exit(1);
 }

 //fcntl(sockfd, F_SETFL, O_NONBLOCK);:q

 // *** Initialize the server socket address ***
 struct sockaddr_in server_addr; // server socket address struct
 server_addr.sin_family = AF_INET; // protocol family
 server_addr.sin_port = htons(port); // port number
 struct hostent *host_name = gethostbyname(hostname); // get IP from host name
 server_addr.sin_addr = *((struct in_addr *)host_name->h_addr_list[0]); // set IP address
 memset(server_addr.sin_zero, '\0', sizeof server_addr.sin_zero); // make the rest bytes zero


 uint16_t server_header[12];
 unsigned int sin_size = sizeof(struct sockaddr_in);
 int SENT_FIN = 0;
 int SENT_FIN_ACK = 0;
 int rcv_fin_ack_server =  0;
 char* msg;
 while(1){
  long datalen =0;

  // *** 3-WAY Handshake ***
  // *** SYN ***
  Header SYN_pkt;
  SEQ_NUM = rand() % MAX_SEQ_NUM;
  uint16_t ACK_NUM;
  createHeader(SEQ_NUM, 0, 1, 0, 0, 0, &SYN_pkt);
  if(syn == 1){
    msg = "RESEND";
  }
  else{
    msg = "SEND";
  }
  printPacket(&SYN_pkt, msg);
  htonHeader(&SYN_pkt);
  uint16_t buf[12];
  memset(buf, '\0', 12);
  bufferHeader(&SYN_pkt, buf);
  sendto(sockfd, buf, 12, 0, (struct sockaddr *)&server_addr, sin_size);
  setTimer(&timeOut);
  int recv_buf_size;
  while(1){

      if ((recv_buf_size = recvfrom(sockfd, server_header, 12, MSG_DONTWAIT,(struct sockaddr *)&server_addr, &sin_size)) > 0){
        Header r_header;
        createHeader(server_header[0],server_header[1],server_header[2],server_header[3],server_header[4], server_header[5], &r_header);
        ntohHeader(&r_header);
        printPacket(&r_header, "RECV");

        // *** ACK + DATA in response to SYN ACK from server ***
        if(hasSYN_ACK(&r_header)){
            Header data_Header;
            SEQ_NUM = r_header.ACK_NUM % MAX_SEQ_NUM;
            ACK_NUM = (r_header.SEQ_NUM+1) % MAX_SEQ_NUM;
            LAST_ACK = r_header.ACK_NUM;
            LAST_NEXT_SEQ_NUM = NEXT_SEQ_NUM;
            if(size < MAX_DATA_SIZE){
                //createHeader(SEQ_NUM,ACK_NUM,0,0,1,size, &data_Header);
                datalen = size;
            }
            else{
                //createHeader(SEQ_NUM,ACK_NUM,0,0,1,MAX_DATA_SIZE, &data_Header);
                datalen = MAX_DATA_SIZE;
            }
            createHeader(SEQ_NUM,ACK_NUM,0,0,1,datalen, &data_Header);
            send_base = SEQ_NUM; //seq num of first packet of window
            NEXT_SEQ_NUM += datalen;
            //printf("%ld\n", size);
            printPacket(&data_Header, "SEND");
            htonHeader(&data_Header);
            memset(buf, '\0', 12);
            bufferHeader(&data_Header, buf);
            sendto(sockfd, buf, 12, 0, (struct sockaddr *)&server_addr, sin_size);
            //setTimer(&timeOut);
            fread(data, 1, datalen, fp);
            sendto(sockfd, data, datalen, 0, (struct sockaddr *)&server_addr, sin_size); //this is the very first packet of the window
            setTimer(&timeOut);
            pktnum = totalpkts - 1;
            pkts_sent = 1;

            //continue;

            // *** SENDING DATA NOW ***
            while(pktnum != 0){

                //already sent max packets in window

                SEQ_NUM = ( SEQ_NUM + NEXT_SEQ_NUM ) % MAX_SEQ_NUM;
                ACK_NUM = 0;
                //printf("total pkts %d and pktnum %d\n", totalpkts, pktnum);
                datalen = initPayload(fp, size, pktnum,totalpkts, data);
                //printf("data len is %ld\n", datalen);
                createHeader(SEQ_NUM,ACK_NUM,0,0,0,datalen, &data_Header);
                NEXT_SEQ_NUM = datalen;
                printPacket(&data_Header, "SEND");
                htonHeader(&data_Header);
                memset(buf, '\0', 12);
                bufferHeader(&data_Header, buf);
                sendto(sockfd, buf, 12, 0, (struct sockaddr *)&server_addr, sin_size);
                sendto(sockfd, data, datalen, 0, (struct sockaddr *)&server_addr, sin_size);
                setTimer(&timeOut);
                pktnum--;
                pkts_sent++;
                if(pkts_sent % MAX_WINDOW_SIZE == 0){
                    break;
                }
            }
            continue;
        }
        else if(hasACK(&r_header)){
            if(SENT_FIN){
                // means client sent FIN and received; now waiting for FIN from server
                SEQ_NUM = r_header.ACK_NUM % MAX_SEQ_NUM;
                rcv_fin_ack_server = 1;
                continue;
            }
            if(r_header.ACK_NUM == LAST_ACK){
                printf("DUP ACK!!!!\n");
                continue; //received a DUP  ACK, wait for timer to go out
            }

            LAST_ACK = r_header.ACK_NUM;
            pkts_sent--; //received one ack so have room to send one more packet in window
            LAST_NEXT_SEQ_NUM = NEXT_SEQ_NUM;
            send_base = LAST_ACK;



            /*if(acknum != totalpkts){
                //didnt receive all acks
                continue;
            }*/

            while(pktnum != 0){

                Header data_Header;
                SEQ_NUM = ( SEQ_NUM + NEXT_SEQ_NUM ) % MAX_SEQ_NUM;
                ACK_NUM = 0;
                //printf("total pkts %d and pktnum %d\n", totalpkts, pktnum);
                datalen = initPayload(fp, size, pktnum,totalpkts, data);
                //printf("data len is %ld\n", datalen);
                createHeader(SEQ_NUM,ACK_NUM,0,0,0,datalen, &data_Header);
                NEXT_SEQ_NUM = datalen;
                printPacket(&data_Header, "SEND");
                htonHeader(&data_Header);
                memset(buf, '\0', 12);
                bufferHeader(&data_Header, buf);
                sendto(sockfd, buf, 12, 0, (struct sockaddr *)&server_addr, sin_size);
                sendto(sockfd, data, datalen, 0, (struct sockaddr *)&server_addr, sin_size);
                pktnum--;
                pkts_sent++;
                setTimer(&timeOut);

                //already sent max packets in window
                if(pkts_sent % MAX_WINDOW_SIZE == 0){
                    break;
                }
             }
             if(pktnum != 0){
                continue;
             }

            // *** FIN *** bc we only have one data transmission so need to send FIN immediately after
            Header FIN_pkt;
            //SEQ_NUM = r_header.ACK_NUM % MAX_SEQ_NUM;
            SEQ_NUM = ( SEQ_NUM + NEXT_SEQ_NUM ) % MAX_SEQ_NUM;
            createHeader(SEQ_NUM, 0, 0, 1, 0, 0, &FIN_pkt);
            printPacket(&FIN_pkt, "SEND");
            htonHeader(&FIN_pkt);
            memset(buf, '\0', 12);
            bufferHeader(&FIN_pkt, buf);
            sendto(sockfd, buf, 12, 0, (struct sockaddr *)&server_addr, sin_size);
            setTimer(&timeOut);
            SENT_FIN = 1;
            continue;
        }

        else if(hasFIN(&r_header)){
            Header ACK_pkt;
            ACK_NUM = (r_header.SEQ_NUM+1) % MAX_SEQ_NUM;
            createHeader(SEQ_NUM, ACK_NUM, 0, 0, 1,0, &ACK_pkt);
            printPacket(&ACK_pkt, "SEND");
            htonHeader(&ACK_pkt);
            memset(buf, '\0', 12);
            bufferHeader(&ACK_pkt, buf);
            sendto(sockfd, buf, 12, 0, (struct sockaddr *)&server_addr, sin_size);
            // WAIT FOR 2 SECONDS *****TO DO!!!**** AND THEN CLOSE
            // *** closing socket after sending data
            setTimer2sec(&timeOut);
            SENT_FIN_ACK = 1;
            continue;

        }

      }//loop ends

      if(isTimeOut(&cur_time, &timeOut)){

           if(SENT_FIN_ACK == 1){
                fclose(fp);
                close(sockfd);
                free(data);
                return 0;
           }
           if(LAST_ACK == 0){
                syn = 1;
                printf("TIMEOUT %hu\n", SEQ_NUM);
                break; // SYN got lost !!!
                //printf("SYN GOT LOST\n");
           }
           if(SENT_FIN == 1){ //FIN got lost
                //printf("FIN GOT LOST\n");
                if(rcv_fin_ack_server){
                    continue;
                }
                Header FIN_pkt;
                printf("TIMEOUT %hu\n", SEQ_NUM);
                //SEQ_NUM = r_header.ACK_NUM % MAX_SEQ_NUM;
                //SEQ_NUM = ( SEQ_NUM + NEXT_SEQ_NUM ) % MAX_SEQ_NUM;
                createHeader(SEQ_NUM, 0, 0, 1, 0, 0, &FIN_pkt);
                printPacket(&FIN_pkt, "RESEND");
                htonHeader(&FIN_pkt);
                memset(buf, '\0', 12);
                bufferHeader(&FIN_pkt, buf);
                sendto(sockfd, buf, 12, 0, (struct sockaddr *)&server_addr, sin_size);
                setTimer(&timeOut);
                SENT_FIN = 1;
                continue;
           }
           //other pkt got lost
           SEQ_NUM = ( LAST_ACK ) % MAX_SEQ_NUM;
           printf("TIMEOUT %hu\n", SEQ_NUM);
           LAST_NEXT_SEQ_NUM = 0;
           pkts_resent = 0;
           uint16_t re_pktnum = totalpkts - (LAST_ACK/MAX_DATA_SIZE);
           //printf("pktnum: %d; now add 1\n", pktnum);
           //pktnum++;
           //pktnum = pktnum + 9;
           if(pktnum + 9 > totalpkts){
                pktnum++;
           }
           else{
                pktnum = pktnum + 9;
           }
           //long offset = (totalpkts - pktnum) * MAX_DATA_SIZE;
           fseek(fp, datalen, ftell(fp));
           while( pktnum !=  0){
               Header data_Header;
               SEQ_NUM = ( SEQ_NUM + LAST_NEXT_SEQ_NUM) % MAX_SEQ_NUM;
               ACK_NUM = 0;
               //printf("total pkts %d and repktnum %d\n", totalpkts, re_pktnum);
               datalen = initPayload(fp, size, pktnum,totalpkts, data);
               //printf("data len is %ld\n", datalen);
               createHeader(SEQ_NUM,ACK_NUM,0,0,0,datalen, &data_Header);
               LAST_NEXT_SEQ_NUM = datalen;
               printPacket(&data_Header, "RESEND");
               htonHeader(&data_Header);
               memset(buf, '\0', 12);
               bufferHeader(&data_Header, buf);
               sendto(sockfd, buf, 12, 0, (struct sockaddr *)&server_addr, sin_size);
               sendto(sockfd, data, datalen, 0, (struct sockaddr *)&server_addr, sin_size);
               re_pktnum--;
               pkts_resent++;
               pktnum--;
               setTimer(&timeOut);

               //already sent max packets in window
               if(pkts_resent % MAX_WINDOW_SIZE == 0){
                   break;
               }
               if(pkts_resent == totalpkts){
                    break;
               }
           }
           continue;
       }
  }
 }

 free(data);
 close(sockfd);
 return 0;
}

int numPkts(long file_size){
    int r_pkts = 0;
    int l_pkts = file_size / MAX_DATA_SIZE;
    if(file_size % 512 != 0){
        r_pkts = 1;
    }
    return (l_pkts + r_pkts);
}


/* Reads to_read bytes from open file fp into buffer buf*/
long initPayload(FILE* fp, long file_size, int pktnum, int totalpkts, char* buf){
      memset(buf, '\0', MAX_DATA_SIZE);
      long read_before = ((totalpkts-pktnum) * MAX_DATA_SIZE);
      long to_read = (pktnum == 1? (file_size - read_before) : MAX_DATA_SIZE);
      //fseek(fp, read_before, SEEK_SET);
      long len = fread(buf, 1, to_read, fp);
      if(len < 0){
        perror("fread call failed\n");
        exit(2);
      }
      return len;
}

