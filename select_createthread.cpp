
#include <WinSock2.h>
#include <Windows.h>
#include <MSWSock.h>
#include <stdio.h>
#include <process.h>
#include <map>
#include "curl.h"
#include "easy.h"
#include "cJSON.h"
#include"parser.h"
#include"tree.h"
#include"iconv.h"
using namespace std;

#pragma comment(lib,"Ws2_32.lib")
#pragma comment(lib,"Mswsock.lib")

int	icount		= 0, datasize = 0, times = 0;
char	* reallocstr	= NULL;
HANDLE  hmuter;


#define PORT			8000
#define BUF_SIZE		4096

#define API_URL(x)			((x)==1?"https://api-cn.faceplusplus.com/facepp/v3/detect":"https://aip.baidubce.com/rest/2.0/face/v3/detect")
#define API_CREATEFS(x)		((x)==1?"https://api-cn.faceplusplus.com/facepp/v3/faceset/create":"https://aip.baidubce.com/rest/2.0/face/v3/faceset/group/add")
#define API_FS_ADDFACE(x)	((x)==1?"https://api-cn.faceplusplus.com/facepp/v3/faceset/addface":"https://aip.baidubce.com/rest/2.0/face/v3/faceset/user/add")
#define API_FS_GETDETAIL(x)	((x)==1?"https://api-cn.faceplusplus.com/facepp/v3/faceset/getdetail":"https://aip.baidubce.com/rest/2.0/face/v3/faceset/group/getusers")
#define API_FS_COMPARE(x)	((x)==1?"https://api-cn.faceplusplus.com/facepp/v3/compare":"https://aip.baidubce.com/rest/2.0/face/v3/match")
#define API_FS_SEARCH(x)    ((x)==1?"https://api-cn.faceplusplus.com/facepp/v3/search":"https://aip.baidubce.com/rest/2.0/face/v3/identify")
#define XML_FILE_PATH       "test.xml"

typedef struct Str_socket
{
	char	reqtype[33];
	char    save[7];
	int     apitype;
	int	    allsize;
	long	piclen[4];
}STR_SOC;

typedef struct  Str_socket_data
{
	STR_SOC head;
	BYTE	* picdata1;
	BYTE	* picdata2;
	char	* message;
}STR_DATA;

typedef struct  Str_msg_head
{
	char	reqtype[33];
	char	save[23];
	int	iresult;
	int	msglen;
}STR_MSG_HEAD;

typedef struct  Str_message
{
	STR_MSG_HEAD	head;
	char		*content;
}STR_MSG;


fd_set socketSet;

/* 获取服务器中返回的数据:face_tokens */
size_t req_reply( void *ptr, size_t size, size_t nmemb, void *stream )
{
	char* p = strstr( (char *) ptr, "\"face_token\":" );
	if ( p != NULL )
	{
		char*temp = strtok( p + 14, "\"" );
		while ( temp )
		{
			*(char * *) stream = temp;
			break;
		}
	}else  {
		*(char * *) stream = (char *) ptr;
	}
	return(size * nmemb);
}

size_t req_add( void *ptr1, size_t size, size_t nmemb, void *stream )
{
	*(char * *) stream = (char *) ptr1;
	return(size * nmemb);
}

size_t req_compare( void *ptr1, size_t size, size_t nmemb, void *stream )
{
	*(char * *) stream = (char *) ptr1;
	return(size * nmemb);
}

void GetSelectStr(char* api_key,char* api_secret,int apitype,char* user,char* filename)
{
	xmlDocPtr doc;           //定义解析文档指针  
	xmlNodePtr node;          //定义结点指针(你需要它为了在各个结点间移动)  
	xmlNodePtr newnode;
	xmlKeepBlanksDefault(0);  //避免将空格当作一个节点  
	char* str=NULL;

	doc=xmlParseFile(filename);

	node=xmlDocGetRootElement(doc);
	if (xmlStrcmp(node->name, BAD_CAST "root"))
	{
		fprintf(stderr,"document of the wrong type, root node != root");
		return;
	}
	node = node->xmlChildrenNode;

	if(apitype==1)
	{
		newnode=node->children;
		while (newnode!=NULL)
		{
			if(strcmp((char*)newnode->children->next->next->children->content,"0")==0)
			{
				strcpy(api_key,(char*)newnode->children->children->content);
				strcpy(api_secret,(char*)newnode->children->next->children->content);
				xmlNodeSetContent(newnode->children->next->next->children,(xmlChar*)"1");
				strcpy(user,(char* )newnode->name);
				break;
			}
			newnode=newnode->next;
		}
	}
	else
	{
		newnode=node->children->next;
		while (newnode!=NULL)
		{
			if(strcmp((char*)newnode->children->next->next->children->content,"0")==0)
			{
				strcpy(api_key,(char*)newnode->children->children->content);
				strcpy(api_secret,(char*)newnode->children->next->children->content);
				xmlNodeSetContent(newnode->children->next->next->children,(xmlChar*)"1");
				strcpy(user,(char* )newnode->name);
				break;
			}
			newnode=newnode->next;
		}
	}
	xmlSaveFile(filename, doc); 
	xmlFreeDoc(doc);
}

void SetSelectStr(char* username,int apitype,char* filename)
{
	xmlDocPtr doc;        //定义结点指针(你需要它为了在各个结点间移动)
	xmlNodePtr node; 
	xmlKeepBlanksDefault(0);  //避免将空格当作一个节点  
	doc=xmlParseFile(filename);
	node=xmlDocGetRootElement(doc);
	node = node->xmlChildrenNode;

	if(apitype==1)
	{
		node=node->children;
		while (node!=NULL)
		{
			if(strcmp((char*)node->name,username) == 0)
			{
				xmlNodeSetContent(node->children->next->next->children,(xmlChar*)"0");
				break;
			}
			node=node->next;
		}
	}
	else
	{
		node=node->children->next;
		while (node!=NULL)
		{
			if(strcmp((char*)node->name,username) == 0)
			{
				xmlNodeSetContent(node->children->next->next->children,(xmlChar*)"0");
				break;
			}
			node=node->next;
		}
	}
	xmlSaveFile(filename, doc); 
	xmlFreeDoc(doc);
}

DWORD WINAPI CreateReciveThread( LPVOID lpParameter )
{
	WaitForSingleObject(hmuter,INFINITE);
	xmlNodePtr *newnode=new xmlNodePtr;
	char* apiselect=NULL;
	SOCKET client = (SOCKET) lpParameter;

	struct curl_httppost	*formpost	= NULL;
	struct curl_httppost	*lastptr	= NULL;

	struct curl_httppost	*addpost	= NULL;
	struct curl_httppost	*lastaddptr	= NULL;

	struct curl_httppost	*comparepost	= NULL;
	struct curl_httppost	*compareptr	= NULL;

	struct curl_httppost	*searchpost		= NULL;
	struct curl_httppost	*searchptr		= NULL;
	char			* face_stoken		= NULL;
	char			* fs_add_msg		= NULL;
	char			* fs_compare_msg	= NULL;
	char			* fs_search_msg		= NULL;
	char			* buf			= NULL;
	CURL* curl=NULL;
	STR_MSG str_msg;
	str_msg.content = NULL;

	char		error[BUF_SIZE];
	CURLcode	res;
	STR_DATA	recdata;
	recdata.picdata1	= NULL;
	recdata.picdata2	= NULL;
	char* temp = new char[1024 * 500];
	memset( temp, 0, sizeof(1024 * 500) );
	char username[8];
	char apikey[36];
	char apisecret[36];

	memset(username,0,8);
	memset(apikey,0,36);
	memset(apisecret,0,36);
	int nRecv = recv( client, temp, 1024 * 500, 0 );
	if ( nRecv > 0 )
	{
		memcpy( &recdata.head, temp, sizeof(STR_SOC) );
		times++;
		if ( times == 1 )
		{
			datasize = recdata.head.allsize;
		}
		if ( nRecv != datasize )
		{
			reallocstr = (char *) realloc( reallocstr, nRecv + icount );
			memcpy( reallocstr + icount, temp, nRecv );
			icount += nRecv;
			if ( datasize == icount )
			{
				memset( &recdata.head, 0, sizeof(STR_SOC) );
				memcpy( &recdata.head, reallocstr, sizeof(STR_SOC) );
				memset( temp, 0, sizeof(1024 * 500) );
				memcpy(temp,reallocstr,datasize);
				free(reallocstr);
				reallocstr=NULL;
				times = 0;
				icount=0;
			}else  {
				return(0);
			}
		}else  {
			times = 0;
		}
		if ( recdata.head.piclen[0] > 0 )
		{
			recdata.picdata1 = new BYTE[recdata.head.piclen[0]];
			memset( recdata.picdata1, 0, recdata.head.piclen[0] );
			memcpy( recdata.picdata1, temp + sizeof(STR_SOC), recdata.head.piclen[0] );
		}
		if ( recdata.head.piclen[1] > 0 )
		{
			recdata.picdata2 = new BYTE[recdata.head.piclen[1]];
			memset( recdata.picdata2, 0, recdata.head.piclen[1] );
			memcpy( recdata.picdata2, temp + sizeof(STR_SOC) + recdata.head.piclen[0], recdata.head.piclen[1] );
		}
		if ( strcmp( recdata.head.reqtype, "c4ca4238a0b923820dcc509a6f75849b" ) == 0 )
		{
			//脸部分析:得到唯一标识:face_token
			GetSelectStr(apikey,apisecret,recdata.head.apitype,username,XML_FILE_PATH);
			curl_formadd(&formpost, &lastptr,
				CURLFORM_COPYNAME, "api_key",
				CURLFORM_COPYCONTENTS, apikey,
				CURLFORM_END );

			curl_formadd(&formpost, &lastptr,
				CURLFORM_COPYNAME, "api_secret",
				CURLFORM_COPYCONTENTS,apisecret,
				CURLFORM_END );

			curl_formadd( &formpost, &lastptr,
				CURLFORM_COPYNAME, "image_file",
				CURLFORM_BUFFER, "imgdata",
				CURLFORM_BUFFERPTR, recdata.picdata1,
				CURLFORM_BUFFERLENGTH, recdata.head.piclen[0],
				CURLFORM_CONTENTTYPE, "multipart/form-data",
				CURLFORM_END );

			curl_formadd(&formpost, &lastptr,
				CURLFORM_COPYNAME, "return_landmark",
				CURLFORM_COPYCONTENTS,"1",
				CURLFORM_END );

			curl_formadd(&formpost, &lastptr,
				CURLFORM_COPYNAME, "return_attributes",
				CURLFORM_COPYCONTENTS,"gender,age,smile,glass,headpose,blur,eyestatus,emotion,facequality,ethnicity,beauty,mouthstatus,eyegaze,skinstatus",
				CURLFORM_END );

			curl = curl_easy_init();
			curl_easy_setopt( curl, CURLOPT_POST, 1 );
			curl_easy_setopt( curl, CURLOPT_URL, API_URL(recdata.head.apitype) );
			curl_easy_setopt( curl, CURLOPT_VERBOSE, 0 );
			curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, false );
			curl_easy_setopt( curl, CURLOPT_HTTPPOST, formpost );
			curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, req_reply );
			curl_easy_setopt( curl, CURLOPT_WRITEDATA, &face_stoken );
			curl_easy_setopt( curl, CURLOPT_ERRORBUFFER, error );
			res = curl_easy_perform( curl );
			if ( res != CURLE_OK )
				printf( "%s\n", error );
			curl_easy_cleanup(curl);
			SetSelectStr(username,recdata.head.apitype,XML_FILE_PATH);

			if ( strstr( face_stoken, "error_message" ) )
			{
				strcpy( str_msg.head.reqtype, recdata.head.reqtype );
				str_msg.head.iresult	= -1;
				str_msg.head.msglen	= strlen( face_stoken );
				str_msg.content		= new char[strlen( face_stoken )];
				memcpy( str_msg.content, face_stoken, strlen( face_stoken ) );
				buf = new char[sizeof(STR_MSG_HEAD) + strlen( face_stoken )];
				memset( buf, 0, sizeof(STR_MSG_HEAD) + strlen( face_stoken ) );
				memcpy( buf, &str_msg.head, sizeof(STR_MSG_HEAD) );
				memcpy( buf + sizeof(STR_MSG_HEAD), str_msg.content, strlen( face_stoken ) );
				send( client, buf, sizeof(STR_MSG_HEAD) + strlen( face_stoken ), 0 );
			}else  {
				//添加到faceset_tokens集合中;
				GetSelectStr(apikey,apisecret,recdata.head.apitype,username,XML_FILE_PATH);

				curl_formadd( &addpost,&lastaddptr,
					CURLFORM_COPYNAME, "api_key",
					CURLFORM_COPYCONTENTS, apikey,
					CURLFORM_END );

				curl_formadd( &addpost,&lastaddptr,
					CURLFORM_COPYNAME, "api_secret",
					CURLFORM_COPYCONTENTS,apisecret,
					CURLFORM_END );

				curl_formadd( &addpost,&lastaddptr,
					CURLFORM_COPYNAME, "outer_id",
					CURLFORM_COPYCONTENTS, "OuterID",
					CURLFORM_END );

				curl_formadd( &addpost,&lastaddptr,
					CURLFORM_COPYNAME, "face_tokens",
					CURLFORM_COPYCONTENTS, face_stoken,
					CURLFORM_END );
				curl = curl_easy_init();
				curl_easy_setopt( curl, CURLOPT_POST, 1 );
				curl_easy_setopt( curl, CURLOPT_URL, API_FS_ADDFACE(recdata.head.apitype) );
				curl_easy_setopt( curl, CURLOPT_VERBOSE, 0 );
				curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, false );
				curl_easy_setopt( curl, CURLOPT_HTTPPOST, addpost );
				curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, req_add );
				curl_easy_setopt( curl, CURLOPT_WRITEDATA, &fs_add_msg );
				curl_easy_setopt( curl, CURLOPT_ERRORBUFFER, error );
				res = curl_easy_perform( curl );
				if ( res != CURLE_OK )
					printf( "%s\n", error );
				if ( strstr( fs_add_msg, "error_message" ) )
				{
					strcpy( str_msg.head.reqtype, recdata.head.reqtype );
					str_msg.head.iresult = -2;
				}else  {
					strcpy( str_msg.head.reqtype, recdata.head.reqtype );
					str_msg.head.iresult = 0;
				}
				str_msg.head.msglen	= strlen( fs_add_msg );
				str_msg.content		= new char[strlen( fs_add_msg )];
				memcpy( str_msg.content, fs_add_msg, strlen( fs_add_msg ) );
				buf = new char[sizeof(STR_MSG_HEAD) + strlen( fs_add_msg )];
				memset( buf, 0, sizeof(STR_MSG_HEAD) + strlen( fs_add_msg ) );
				memcpy( buf, &str_msg.head, sizeof(STR_MSG_HEAD) );
				memcpy( buf + sizeof(STR_MSG_HEAD), str_msg.content, strlen( fs_add_msg ) );
				send( client, buf, sizeof(STR_MSG_HEAD) + strlen( fs_add_msg ), 0 );
				curl_formfree(formpost);
				curl_formfree(addpost);
				curl_easy_cleanup(curl);
				SetSelectStr(username,recdata.head.apitype,XML_FILE_PATH);
			}
		}else if ( strcmp( recdata.head.reqtype, "c81e728d9d4c2f636f067f89cc14862c" ) == 0 )
		{
			/* 对比 */

			GetSelectStr(apikey,apisecret,recdata.head.apitype,username,XML_FILE_PATH);

			curl_formadd(&comparepost, &compareptr,
				CURLFORM_COPYNAME, "api_key",
				CURLFORM_COPYCONTENTS, apikey,
				CURLFORM_END );

			curl_formadd( &comparepost, &compareptr,
				CURLFORM_COPYNAME, "api_secret",
				CURLFORM_COPYCONTENTS,apisecret,
				CURLFORM_END );

			curl_formadd( &comparepost, &compareptr,
				CURLFORM_COPYNAME, "image_file1",
				CURLFORM_BUFFER, "imgdata1",
				CURLFORM_BUFFERPTR, recdata.picdata1,
				CURLFORM_BUFFERLENGTH, recdata.head.piclen[0],
				CURLFORM_CONTENTTYPE, "multipart/form-data",
				CURLFORM_END );

			curl_formadd( &comparepost, &compareptr,
				CURLFORM_COPYNAME, "image_file2",
				CURLFORM_BUFFER, "imgdata2",
				CURLFORM_BUFFERPTR, recdata.picdata2,
				CURLFORM_BUFFERLENGTH, recdata.head.piclen[1],
				CURLFORM_CONTENTTYPE, "multipart/form-data",
				CURLFORM_END );

			curl = curl_easy_init();
			curl_easy_setopt( curl, CURLOPT_POST, 1 );
			curl_easy_setopt( curl, CURLOPT_URL, API_FS_COMPARE(recdata.head.apitype));
			curl_easy_setopt( curl, CURLOPT_VERBOSE, 0 );
			curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, false );
			curl_easy_setopt( curl, CURLOPT_HTTPPOST, comparepost );
			curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, req_compare );
			curl_easy_setopt( curl, CURLOPT_WRITEDATA, &fs_compare_msg );

			curl_easy_setopt( curl, CURLOPT_ERRORBUFFER, error );

			res = curl_easy_perform( curl );
			if ( res != CURLE_OK )
				printf( "%s\n", error );

			if ( strstr( fs_compare_msg, "error_message" ) )
			{
				strcpy( str_msg.head.reqtype, recdata.head.reqtype );
				str_msg.head.iresult = -1;
			}else  {
				strcpy( str_msg.head.reqtype, recdata.head.reqtype );
				str_msg.head.iresult = 0;
			}

			str_msg.head.msglen	= strlen( fs_compare_msg );
			str_msg.content		= new char[strlen( fs_compare_msg )];
			memset( str_msg.content, 0, str_msg.head.msglen );
			memcpy( str_msg.content, fs_compare_msg, strlen( fs_compare_msg ) );
			buf = new char[sizeof(STR_MSG_HEAD) + strlen( fs_compare_msg )];
			memset( buf, 0, sizeof(STR_MSG_HEAD) + strlen( fs_compare_msg ) );
			memcpy( buf, &str_msg.head, sizeof(STR_MSG_HEAD) );
			memcpy( buf + sizeof(STR_MSG_HEAD), str_msg.content, strlen( fs_compare_msg ) );
			send( client, buf, sizeof(STR_MSG_HEAD) + strlen( fs_compare_msg ), 0 );
			curl_formfree(comparepost);
			curl_easy_cleanup(curl);
			SetSelectStr(username,recdata.head.apitype,XML_FILE_PATH);

		}else if ( strcmp( recdata.head.reqtype, "eccbc87e4b5ce2fe28308fd9f2a7baf3" ) == 0 )
		{
			/* 搜索 */
			GetSelectStr(apikey,apisecret,recdata.head.apitype,username,XML_FILE_PATH);
			//printf("user=%s\napikey=%s\napisecret=%s\n",username,apikey,apisecret);
			//1:face++的参数格式
			if(recdata.head.apitype==1)
			{
				curl_formadd( &searchpost, &searchptr,
					CURLFORM_COPYNAME, "api_key",
					CURLFORM_COPYCONTENTS, apikey,
					CURLFORM_END );

				curl_formadd( &searchpost, &searchptr,
					CURLFORM_COPYNAME, "api_secret",
					CURLFORM_COPYCONTENTS,apisecret,
					CURLFORM_END );

				curl_formadd( &searchpost, &searchptr,
					CURLFORM_COPYNAME, "image_file",
					CURLFORM_BUFFER, "imgdata1",
					CURLFORM_BUFFERPTR, recdata.picdata1,
					CURLFORM_BUFFERLENGTH, recdata.head.piclen[0],
					CURLFORM_CONTENTTYPE, "multipart/form-data",
					CURLFORM_END );

				curl_formadd( &searchpost, &searchptr,
					CURLFORM_COPYNAME, "outer_id",
					CURLFORM_COPYCONTENTS, "OuterID",
					CURLFORM_END );

				curl_formadd( &searchpost, &searchptr,
					CURLFORM_COPYNAME, "return_result_count",
					CURLFORM_COPYCONTENTS, "1",
					CURLFORM_END );
			}
			//其他:百度的参数格式 urlencode格式
			else
			{
				//
			}
			curl = curl_easy_init();
			curl_easy_setopt( curl, CURLOPT_POST, 1 );
			curl_easy_setopt( curl, CURLOPT_URL, API_FS_SEARCH(recdata.head.apitype) );
			curl_easy_setopt( curl, CURLOPT_VERBOSE, 0 );
			curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, false );
			curl_easy_setopt( curl, CURLOPT_HTTPPOST, searchpost );
			curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, req_compare );
			curl_easy_setopt( curl, CURLOPT_WRITEDATA, &fs_search_msg );
			curl_easy_setopt( curl, CURLOPT_ERRORBUFFER, error );
			res = curl_easy_perform( curl );
			if ( res != CURLE_OK )
				printf( "%s\n", error );

			if ( strstr( fs_search_msg, "error_message" ) )
			{
				strcpy( str_msg.head.reqtype, recdata.head.reqtype );
				str_msg.head.iresult = -1;
			}else  {
				strcpy( str_msg.head.reqtype, recdata.head.reqtype );
				str_msg.head.iresult = 0;
			}
			str_msg.head.msglen	= strlen( fs_search_msg );
			str_msg.content		= new char[strlen( fs_search_msg )];
			memset( str_msg.content, 0, str_msg.head.msglen );
			memcpy( str_msg.content, fs_search_msg, strlen( fs_search_msg ) );
			buf = new char[sizeof(STR_MSG_HEAD) + strlen( fs_search_msg )];
			memset( buf, 0, sizeof(STR_MSG_HEAD) + strlen( fs_search_msg ) );
			memcpy( buf, &str_msg.head, sizeof(STR_MSG_HEAD) );
			memcpy( buf + sizeof(STR_MSG_HEAD), str_msg.content, strlen( fs_search_msg ) );
			send( client, buf, sizeof(STR_MSG_HEAD) + strlen( fs_search_msg ), 0 );
			curl_easy_cleanup(curl);
			//Sleep(1000);
			SetSelectStr(username,recdata.head.apitype,XML_FILE_PATH);
		}else  {
			/*
			* //发送其他数据
			* int nRet = send(client,buf, strlen(buf)+1 ,0);
			* if(nRet <= 0)
			* {
			* if(GetLastError() == WSAEWOULDBLOCK)
			* {
			*      //do nothing
			* }
			* else
			* {
			*      closesocket(client);
			* }
			* }
			*/
		}

	}else if ( nRecv == 0 )
	{
		FD_CLR( client, &socketSet );
		closesocket( client );
	}else if ( nRecv == -1 && client!=socketSet.fd_array[0] )
	{
		FD_CLR( client, &socketSet );
		closesocket( client );
	}
	delete[] buf;
	delete[] str_msg.content;
	delete[] temp;

	delete[] recdata.picdata1;
	delete[] recdata.picdata2;
	ReleaseMutex(hmuter);
	return(0);
}

int main()
{
	try {
		WSAData wsaData;
		u_long	ul = 1;
		HANDLE	hthread;
		hmuter=CreateMutex(NULL,false,NULL);

		if ( 0 != WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) )
		{
			printf( "init failed![%d]\n", WSAGetLastError() );
			return(-1);
		}

		SOCKET sListen = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
		if ( sListen == INVALID_SOCKET )
		{
			printf( "socket failed![%d]\n", WSAGetLastError() );
			return(-1);
		}
		//设置为非阻塞模式
		//ioctlsocket( sListen, FIONBIO, &ul );

		sockaddr_in sin;
		sin.sin_family			= AF_INET;
		sin.sin_port			= htons( PORT );
		sin.sin_addr.S_un.S_addr	= ADDR_ANY;

		if ( SOCKET_ERROR == bind( sListen, (sockaddr *) &sin, sizeof(sin) ) )
		{
			printf( "bind failed![%d]\n", WSAGetLastError() );
			closesocket( sListen );
			WSACleanup();
			return(-1);
		}

		if ( SOCKET_ERROR == listen( sListen, SOMAXCONN ) )
		{
			printf( "listen failed![%d]\n", WSAGetLastError() );
			closesocket( sListen );
			WSACleanup();
			return(-1);
		}

		TIMEVAL time = { 1, 0 };

		FD_ZERO( &socketSet );
		FD_SET( sListen, &socketSet );

		fd_set readSet;
		FD_ZERO( &readSet );

		while ( true )
		{
			readSet = socketSet;
			int nRetAll = select( 0, &readSet, NULL, NULL, &time );
			if ( nRetAll > 0 )
			{
				if ( FD_ISSET( sListen, &readSet ) )
				{
					if ( socketSet.fd_count < FD_SETSIZE )
					{
						sockaddr_in	addrRemote;
						int		nAddrLen	= sizeof(addrRemote);
						SOCKET		sClient		= accept( sListen, (sockaddr *) &addrRemote, &nAddrLen );

						if ( sClient != INVALID_SOCKET )
						{
							FD_SET( sClient, &socketSet );
						}
					}
					else  
					{
						continue;
					}
				}
				for ( unsigned int i = 0; i < socketSet.fd_count; i++ )
				{
					if ( FD_ISSET( socketSet.fd_array[i], &readSet ) )
					{
						hthread = CreateThread( NULL, 0, CreateReciveThread, (LPVOID) socketSet.fd_array[i], 0, NULL );
						WaitForSingleObject( hthread, INFINITE );
						CloseHandle( hthread );
					}
				}
			}else if ( nRetAll == 0 )
			{
				/* printf("time out!\n"); */
			}else  
			{
				break;
			}
		}
		closesocket( sListen );
		CloseHandle(hmuter);
		WSACleanup();
	}
	catch( exception & e )
	{
		printf( "%s\n", e.what() );
	}
	return(0);
}