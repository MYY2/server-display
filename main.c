#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <netdb.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#define MIME_VERSION "MIME-Version: 1.0"
#define HEADER_TYPE "Content-type: multipart/alternative;"
#define CONTENT_TRANSFER_ENCODING "Content-Transfer-Encoding:"
#define CONTENT_TYPE "Content-Type:"
#define CHARSET "charset=UTF-8\r\n"
#define MAX 999999
#define TWO 2
#define THREE 3
#define STATUS2 2
#define STATUS3 3
#define FOUR 4
#define EIGHT 8
#define STATUS4 4
#define STATUS0 0
#define STATUS1 1
#define RUN 1

/* this method is going to determine whether there is a string match into the line of message 
 ignoring the upper and lower case*/
int case_insensitive_compare(char *line, char *needle) {
    int needle_len = strlen(needle);
    for (int i = 0; i <= strlen(line) - needle_len; i++) {
        int count= 0 ;
        for (int j = 0; j < needle_len; j++) {
            if (tolower(line[i + j]) != tolower(needle[j])) {
                break;
            }
            count++;
        }
        if (count == needle_len) {
            return 1;
        }
    }

    return 0;
}


int isNumber(char *str){
    if(*str == '\0'){
        return -1;
    }
    if(*str == '+' || *str == '-'){
        str++;
    }
    while(*str != '\0'){
        if(!isdigit(*str)){
            return -1;
        }
        str++;
    }
    return 1;
}

void sigpipe(int sig) {
    printf("SIGPIPE\n");
    exit(STATUS2); 
}


int create_socket(char* serverName){
    int sockfd;
    int s;
    char* port = "143";
    struct addrinfo hints, *servinfo, *rp;
    memset(&hints,0,sizeof hints);
    // get IPv4 or IPv6
    hints.ai_family = AF_INET6;
    // TCP
    hints.ai_socktype = SOCK_STREAM; 
    s = getaddrinfo(serverName, port, &hints, &servinfo);
    if (s != 0) {
        hints.ai_family = AF_INET;
        s = getaddrinfo(serverName, port, &hints, &servinfo);
        if(s != 0){
            fprintf(stderr,"getaddrinfo: %s\n", gai_strerror(s));
            exit(EXIT_FAILURE);
        }
    }
    for(rp = servinfo; rp != NULL; rp = rp->ai_next){
        //creat client side socket
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(sockfd == -1){
            continue;
        }
        //server side socket correct
        int connect_num = connect(sockfd,rp->ai_addr,rp->ai_addrlen);
        if(connect_num != -1){
            break;
        }
        close(sockfd);
    }
    if(rp == NULL){
        fprintf(stderr,"fail\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(servinfo);
    return sockfd;
}

int recieve(int sockfd,char *read_buffer, size_t size){
    ssize_t size_s = read(sockfd,read_buffer,size-1);
    //if the reading not valid
    if(size_s < 0){
        printf("fail reading\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    //if the reading is sucessed
    read_buffer[size_s] = '\0';
    return 0;
}

int send_message(int sockfd,char *content){
    if(write(sockfd,content,strlen(content)) < 0){
        perror("Error");
        return -1;
    }
    return 0;
}


void do_retrieve(int sockfd, char* messageNum){
    char write_retrieve[MAX] = {0};
    snprintf(write_retrieve, MAX, "A03 FETCH %s BODY.PEEK[]\r\n", messageNum);
    send_message(sockfd, write_retrieve);
    char num[MAX] = {0};
    char one_char;
    int num_position = 0;
    //read the first char
    read(sockfd, &one_char, 1);
    //when not a message will be a A03 at first
    if(one_char == 'A'){
        printf("Message not found\n");
        close(sockfd);
        exit(STATUS3);
    }
    //success fetch a message and find the total number of char in the {}
    for(int i = 0; i < MAX; i++){
        read(sockfd, &one_char, 1);
        //fine the start {
        if(one_char == '{'){
            for(int j = i + 1; j < MAX; j++){
                read(sockfd, &one_char, 1);
                //end }
                if(one_char == '}'){
                    break;
                }
                //record the number
                num[num_position] = one_char;
                num_position++;
            }
            break;
        }
    }
    int tot_num = 0;
    if(isNumber(num) == 1){
        tot_num = atoi(num);
    }
    //skip the \r\n after the } and print the result with total num just found
    for(int i = 0;i < tot_num + TWO; i++){
        read(sockfd, &one_char, sizeof(char));
        if(i != 0 && i != 1){
            printf("%c", one_char);
        }
    }
}

void do_parse(int sockfd,char* messageNum){
    char write_parse[MAX] = {0};
    char read_parse[MAX] = {0};
    char strings[FOUR][EIGHT] = {0};
    //initial the 2D array use for looping
    strcpy(strings[0], "From");
    strcpy(strings[1], "To");
    strcpy(strings[TWO], "Date");
    strcpy(strings[THREE], "Subject");
    //loop by order of the strings
    for(int k = 0; k < FOUR ; k++){
        snprintf(write_parse, MAX, "A03 FETCH %s BODY.PEEK[HEADER.FIELDS (%s)]\r\n", messageNum,strings[k]);
        send_message(sockfd, write_parse);
        recieve(sockfd, read_parse, MAX);
        //if there is not message at the messagenum
        if(strncmp(read_parse, "A03 BAD", strlen("A03 BAD")) == 0){
            printf("Message not found\n");
            close(sockfd);
            exit(STATUS3);
            break;
        }
        printf("%s", strings[k]);
        for(int i = 0; i < MAX; i++){
            //when no To header;
            if(read_parse[i]=='}'&&read_parse[i + THREE] == '\r' && strcmp(strings[k], "To") == 0){
                printf(":\n");
                break;
            }
            //when no Subject header;
            if(read_parse[i] == '}' && read_parse[i + THREE] == '\r' && strcmp(strings[k], "Subject") == 0){
                printf(": <No subject>\n");
                break;
            }
            if(read_parse[i] == ':'){
                for(int j = i ; j < MAX; j++){
                    //still in the same line
                    if(read_parse[j] != '\r'){
                        printf("%c", read_parse[j]);
                    }else if(read_parse[j] == '\r' && read_parse[j + TWO] == '\r'){
                        //when one line finishes and the next line is empty, all finished
                        printf("\n");
                        break;
                    }else{
                        //keep type the next line
                        j = j + 1;
                    }
                }
                break;
            }
        }
    }
}
//check whether this line is match the correct type or not
int match_content_transfer_encoding(char *each_line){
    if(case_insensitive_compare(each_line, "quoted-printable") == 0){
        return 1;
    }
    if(case_insensitive_compare(each_line, "7bit") == 0){
        return 1;
    }
    if(case_insensitive_compare(each_line, "8bit") == 0){
        return 1;
    }
    return 0;
}

// create the end boundary when the boundary is found
void create_end_boundary(char* begin_boundary, char* end_boundary){
    size_t len = strlen(begin_boundary);
    strcpy(end_boundary, begin_boundary);
    end_boundary[len] = '-';
    end_boundary[len + 1] = '-';
    end_boundary[len + TWO] = '\0';
}
// create the begin boundary when the boundary is found
void create_begin_boundary(char *each_line, char *boundary, int stringlen){
    boundary[0] = '-';
    boundary[1] = '-';
    int count = TWO;
    int double_colon = 0;
    int single_colon = 0;
    size_t array_size = strlen(each_line);
    for(int i = stringlen; i < array_size; i++){
        if(each_line[i] == '\r' && each_line[i + 1] == '\n'){
            break;
        }
        if(each_line[i] == '"'){
            double_colon++;
            if(double_colon >= TWO){
                break;
            }
            continue;   
        }
        if(each_line[i] == '\''){
            single_colon++;
            if(single_colon >= TWO){
                break;
            }  
            continue;   
        }
        boundary[count] = each_line[i];
        count++;
    }
    boundary[count] = '\0';
}
//check if the header is match, otherwise the file is not correct
int match_header(char *each_line,int match, int *version, int *header_type){
    //if the version is corrct
    if(strncasecmp(each_line,MIME_VERSION, strlen(MIME_VERSION)) == 0 && match == 0 && (strstr(each_line,"(") == NULL ||strstr(each_line,")") == NULL) ){
        *version = 1;
    }
    //if the header content type is corrct
    if(strncasecmp(each_line,HEADER_TYPE,strlen(HEADER_TYPE)) == 0){
        *header_type = 1;
    }

    if(*version == 1 && *header_type == 1){
        return 1;
    }
    return 0;
}

void do_mime(int sockfd, char* messageNum){
    char write[MAX] = {0};
    int version = 0;
    int header_type = 0;
    snprintf(write, MAX, "A03 FETCH %s BODY.PEEK[]\r\n", messageNum);
    ssize_t n = send_message(sockfd, write);
    //if the message is failed to send
    if(n < 0){
        printf("error!\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char *boundary_string = " boundary=";
    char begin_boundary[MAX] = {0};
    char end_boundary[MAX] = {0};
    int body_section = 0;
    int match = 0;
    int is_content_type = 0;
    int is_CTE = 0;
    int is_type = 0;
    char *type = "text/plain;";
    int is_charset = 0;
    int is_null_line = 0;
    char *null_line = "\r\n";
    int found_text = 0;
    char last[MAX] = {0};
    int line_count = 0;
    int header = 0;
    //to find boundary value
    //read one line
    while(RUN){
        int count = 0;
        char each_line[MAX] = {0};
        char last_char = '\0';
        //get one line message
        while(RUN){
            char now_char;
            read(sockfd, &now_char, sizeof(char));
            each_line[count] = now_char;
            count++;
            if(last_char =='\0'){
                last_char = now_char;
            }
            //if there is the end of a line
            if(now_char == '\n' && last_char == '\r'){
                break;
            }
            last_char = now_char;
        }
        line_count++;
        //if not get the message number
        if(strncasecmp(each_line, "A03 BAD", strlen("A03 BAD")) == 0 && line_count == 1){
            printf("Message not found\n");
            close(sockfd);
            exit(STATUS3);
        }

        header = match_header(each_line, match,& version, & header_type);
        if(header == 1){
            if(strncasecmp(each_line, boundary_string, strlen(boundary_string)) == 0){
                //this line is fit the boundary type, collect the boundary
                create_begin_boundary(each_line, begin_boundary, strlen(boundary_string));
                match = 1;
                // create the end boundary type
                create_end_boundary(begin_boundary, end_boundary);
                continue;
            }
            //if it is end
            if(match == 1 && body_section == 1 && strncasecmp(each_line, end_boundary, strlen(end_boundary)) == 0){
                body_section = 0;
                match = 0;
                break;
            }
            //if it is in the body
            if(match == 1 && body_section == 1){
                //if satisfy all the component that specified
                if(is_content_type == 1 && is_CTE == 1 && is_type == 1 && is_charset == 1 && is_null_line == 1){
                    //if this line is the first line
                    if(last[0] == '\0'){
                        strcpy(last,each_line);
                    }else{
                        //if there is end of valid text,no need to print latest line
                        if(strncasecmp(each_line, begin_boundary, strlen(begin_boundary)) == 0){
                            found_text = 1;
                            break;
                        //if there is in the valid text, then print the latest line
                        }else{
                            printf("%s", last);
                            strcpy(last, each_line);
                        }
                    }
                    continue;
                }
                //if this line is null line
                if(strcasecmp(each_line, null_line) == 0){
                        //if it is the last component
                    if(is_null_line == 0 && is_content_type == 1 && is_CTE == 1 && is_type == 1 && is_charset == 1){
                        is_null_line = 1;
                        continue;
                    }else{
                        //search the right body again
                        is_content_type = 0;
                        is_CTE = 0;
                        is_type = 0;
                        is_charset = 0;
                        //here is  not right body_section
                        body_section = 0;
                        continue;
                    }
                }
                //if the component is matched 
                if(case_insensitive_compare(each_line, type) == 1){
                    is_type = 1;
                }
                if(case_insensitive_compare(each_line, CHARSET) == 1){
                    is_charset = 1;
                }
                if(strncasecmp(each_line, CONTENT_TYPE, strlen(CONTENT_TYPE)) == 0){
                    is_content_type = 1;
                }
                if(strncasecmp(each_line, CONTENT_TRANSFER_ENCODING, strlen(CONTENT_TRANSFER_ENCODING)) == 0 && (match_content_transfer_encoding(each_line) == 1)){
                    is_CTE = 1;
                }
            }
            //if it is the body begin
            if(match == 1 && strncasecmp(each_line, begin_boundary, strlen(begin_boundary)) == 0){
                body_section = 1;
                continue;
            }
        }

    }

    //no correct text found
    if(found_text == 0 || header == 0){
        printf("Matching fails\n");
        close(sockfd);
        exit(STATUS4);
    }
}


void do_list(int sockfd, char* messageNum){
    char write[MAX] = {0};
    char read_file[MAX] = {0};
    snprintf(write, MAX, "A07 SEARCH ALL\r\n");
    ssize_t n = send_message(sockfd, write);
    if(n<0){
        printf("error\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    char read[MAX] = {0};
    n = recieve(sockfd, read, MAX);
    int count = 0;

    while(RUN){
        //catch the first line
        if(read[count]=='\r'&& read[count+1]=='\n'){
            break;
        }
        count++;
    }
    read[count]='\0';
    char *token;
    // get the token
    token = strtok(read, " ");
    int token_count = 0;
    while (token != NULL) {
        //if the token is the sequence number
        if(token_count > 1){
            snprintf(write, MAX, "A08 FETCH %s BODY.PEEK[HEADER.FIELDS (SUBJECT)]\r\n", token);
            send_message(sockfd, write);
            //read in different file that to keep the token tracking
            recieve(sockfd, read_file, MAX);
            printf("%s: ",token);
            for(int i = 0; i < MAX; i++){
                //if empty after the first line, nothing here
                if(read_file[i] == '}'&& read_file[i + THREE] == '\r'){
                    printf("<No subject>\n");
                    break;
                }
                //after read a :
                if(read_file[i] == ':'){
                    for(int k = i + 1; k < MAX; k++){
                        //when read a char not space start
                        if(read_file[k] != ' '){
                            for(int j = k; j < MAX; j++){
                                //when a space
                                if(read_file[j] == ' '){
                                    //count the number of space
                                    int count_space = 1;
                                    for(int n = j + 1; n < MAX; n++){
                                        if(read_file[n] == ' '){
                                            count_space++;
                                        }else if(read_file[n] != ' ' && read_file[n] != '\r'){
                                            //when read a char before read a \r
                                            for(int m = 0; m < count_space; m++){
                                                //print all space i count
                                                printf(" ");
                                            }
                                            //let the next looking index at n
                                            j = n - 1;
                                            break;
                                        }else if(read_file[n] != ' '&& read_file[n] == '\r' && read_file[n + TWO] != '\r'){
                                            //when read a \r but there are still next line
                                            for(int m = 0; m < count_space; m++){
                                                //print all space
                                                printf(" ");
                                            }
                                            //let the next looking index at n
                                            j = n-1;
                                            break;
                                        }else if(read_file[n]!=' '&& read_file[n]=='\r' && read_file[n + TWO]=='\r'){
                                            //when read a \r but no next line, no trailing whitespace
                                            //let the next looking index at n
                                            j = n-1;
                                            break;
                                        }
                                    }
                                    continue;
                                }
                                //read a char
                                if(read_file[j] != '\r'){
                                    printf("%c", read_file[j]);
                                }else if(read_file[j] == '\r' && read_file[j + TWO] == '\r'){
                                    //there is not next line
                                    printf("\n");
                                    break;
                                }else{
                                    //there is next line, continue to next line
                                    j=j+1;
                                }
                            }
                            break;    
                        }
                            
                    }
                    break;
                }
            }    
        }
        //get next token
        token = strtok(NULL, " ");
        token_count++;
    }

}



int main(int argc, char** argv) {
    //initial char
    char* username = "\0";
    char* password = "\0";
    char* folder = "\0";
    char* messageNum = "\0";
    char* command = "\0";
    char* server_name = "\0";
    int sockfd;
    int have_f = 0;
    int have_m =0;
    //lookup argv for command line
    for(int i = 0; i < argc-TWO; i++){
        if(strcmp(argv[i], "-u") == 0){
            //make sure i+1 in range
            if(i+1<argc-TWO){
                username = argv[i + 1];
                i++;
            }
            continue;
        }
        if(strcmp(argv[i],"-p") == 0){
            //make sure i+1 in range
            if(i+1<argc-TWO){
                password = argv[i + 1];
                i++;
            }
            continue;
        }
        if(strcmp(argv[i], "-f") == 0){
            have_f = 1;
            //make sure i+1 in range
            if(i + 1 < argc - TWO){
                folder = argv[i + 1];
                i++;
            }
            continue;
        }
        if(strcmp(argv[i], "-n") == 0){
            have_m = 1;
            //make sure i+1 in range
            if(i + 1 < argc - TWO){
                //make sure input is a number
                if(isNumber(argv[i + 1]) == 1){
                    //make sure the number is a valid messageNum
                    if(atoi(argv[i + 1]) > 0){
                        messageNum = argv[i + 1];
                        i++;
                    }
                }
            }
            continue;
        }
    }
    //when not read a -f
    if(have_f == 0){
        folder="INBOX";
    }
    command = argv[argc - TWO];
    server_name = argv[argc - 1];
    //when unlegal input
    if(strcmp(username,"\0") == 0 || strcmp(password, "\0") == 0 ||
        strcmp(command, "\0") == 0||strcmp(server_name, "\0") == 0||
        (have_f == 1 && strcmp(folder,"\0") == 0)||
        (have_m == 1 && strcmp(messageNum,"\0") == 0)){
        exit(STATUS1);
    }
    //get socket id
    sockfd = create_socket(server_name);
    //signal
    signal(SIGPIPE, sigpipe);


    char read[MAX]={0};
    //read message from reading buffer
    recieve(sockfd, read, MAX);
    //write the message in write file
    char write[MAX]={0};
    //need to wirte tage and login detail to server socket
    snprintf(write,MAX,"A01 LOGIN \"%s\" \"%s\"\r\n", username, password); 
    send_message(sockfd, write);
    //read the recieve message from server, login success
    recieve(sockfd, read, MAX); 
    //login fail
    if(strncmp(read, "A01 NO", strlen("A01 NO")) == 0){
        printf("Login failure\n");
        close(sockfd);
        exit(STATUS3);
    }

    // write the message in the input folder
    snprintf(write,MAX,"A02 SELECT \"%s\"\r\n",folder);
    send_message(sockfd, write);
    // recieve server's response
    recieve(sockfd, read, MAX); 
    //if the folder does not exist
    if(strncmp(read, "A02 NO", strlen("A02 NO")) == 0){
        printf("Folder not found\n");
        close(sockfd);
        exit(STATUS3);
    }
    //if messageNum not specified on the command line
    if(strcmp(messageNum,"\0") == 0 && have_m ==0){
        //find the total num of messages
        snprintf(write, MAX, "A06 STATUS \"%s\" (MESSAGES)\r\n",folder);
        send_message(sockfd,write);
        recieve(sockfd, read, MAX);
        char *ptr = strstr(read, "MESSAGES");
        if (ptr != NULL) {
            int messages;
            sscanf(ptr, "MESSAGES %d", &messages);
            char number[MAX] = {0};
            sprintf(number, "%d", messages);
            //let messageNum is the last added message
            messageNum = number;
        } else {
            close(sockfd);
            exit(STATUS1);
        }
    }   
    //task2.4 retrieve
    if(strcmp(command,"retrieve")==0){
        do_retrieve(sockfd,messageNum);
        close(sockfd);
        exit(STATUS0);
    }
    //task 2.5 parse
    if(strcmp(command,"parse")==0){
        do_parse(sockfd,messageNum);
        close(sockfd);
        exit(STATUS0);
    }
    //task 2.6 mime
    if(strcmp(command,"mime")==0){
        do_mime(sockfd, messageNum);
        close(sockfd);
        exit(STATUS0);
    }
    //task 2.7 list
    if(strcmp(command,"list")==0){
        do_list(sockfd, messageNum);
        close(sockfd);
        exit(STATUS0);
    }
}