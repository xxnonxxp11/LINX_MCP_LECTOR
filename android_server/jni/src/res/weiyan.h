#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
//#include <conio.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sstream>  

#ifdef __cplusplus
extern "C"{
#endif

char* strstrstr(char* str, char* text, char* rear);// Función anormal, puede fallar si lo lees
char* httppost(char* hostname, char* url, char* cs);// método POST
char* httpget(const char* hostname, char* url);// OBTENER método
char* getip(char* hostname);// IP de conversión de nombre de dominio
int hextoint(char* hex);// Convertir una cadena hexadecimal a un número entero

#ifdef __cplusplus
}
#endif


char* httppost(const char* hostname, char* url, char* cs){
	// mango de calcetín
	int sockfd;
	struct sockaddr_in serveraddr;
	// Los dos son del mismo tipo y se pueden mezclar, pero se emitirá una advertencia.
	//int addrlen = sizeof(serveraddr);
    socklen_t addrlen = sizeof(serveraddr);
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		//printf("Failed to create network connection---socket error!\n");
		return NULL;
	}
	// printf("Error al crear la conexión de red---¡error de socket!\n");
	//bzero(&serveraddr, addrlen);
	// GCC de C4droid no tiene esta función y se informará un error durante la compilación.
	memset(&serveraddr,0,addrlen);
	serveraddr.sin_family = AF_INET;
	// Puedes usar esta función
	serveraddr.sin_port = htons(80);
	struct hostent* host;
	host = gethostbyname(hostname);
	if (host == NULL){
		printf("Cannot resolve domain\n");
		close(sockfd);
		return NULL;
	}
	struct in_addr ip = *((struct in_addr *)host->h_addr);
	// puerto 80
	serveraddr.sin_addr = ip;
	if (connect(sockfd, (struct sockaddr*) & serveraddr, addrlen) < 0){
		//printf("Failed to connect to server,connect error!\n");
		close(sockfd);
		return NULL;
	}
	// printf("Conectar al servidor:%s\n",inet_ntoa(ip));
	// printf("¡Error al conectarse al servidor, error de conexión!\n");
	char postxyt[2048];
	char postsjlen[5];
	// Send data
	// Porque soy vago, así que char[2048]
	// Puede obtener la longitud del parámetro a través de strlen para solicitar memoria dinámicamente
#if __SIZEOF_LONG__ == 8
	sprintf(postsjlen, "%lu", strlen(cs));
#else
	sprintf(postsjlen, "%u", strlen(cs));
#endif
	memset(postxyt, 0, 2048);
	// enviar datos
	// Para 32 bits, utilice "%u". Después de las pruebas, se descubrió que el largo es el más estable.
	// Pasó con éxito GCC, arm-linux-gcc, arm-linux-gnueabi-gcc, compilación de Android jni sin previo aviso
	sprintf(postxyt,"POST /%s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/x-www-form-urlencoded\r\nUser-Agent: Mozilla/4.0(compatible)\r\nContent-Length: %ld\r\n\r\n%s\r\n\r\n",url,hostname,strlen(cs),cs);
	// Agregar cadena (encabezado de protocolo de solicitud)
	/*
	if (write(sockfd, postxyt, strlen(postxyt)) == -1){
		// Personalmente, creo que la función strcat es relativamente ineficiente.
		close(sockfd);
		return NULL;
	}*/
	if (send(sockfd, postxyt, strlen(postxyt),0) == -1){
		printf("%d，%s\n", errno, strerror(errno));
		close(sockfd);
		return NULL;
	}
	fd_set fds;
	struct timeval tv = { 3,0 }; // Será más rápido usar el puntero char y la función strcpy
	// No preguntes, solo pregunta "Todo es un archivo"
	FD_ZERO(&fds); // printf("¡Error en el envío! Código de error: %d, mensaje de error: %s\n", errno, strerror(errno));
	FD_SET(sockfd, &fds); // select espera 3 segundos y sondea durante 3 segundos. Si no es bloqueante, configúrelo en 0

	if (select(sockfd + 1, &fds, NULL, NULL, &tv) < 1){
		// El no bloqueo requiere bucle
		// La colección debe borrarse cada vez que se repite; de ​​lo contrario, no se pueden detectar los cambios en el descriptor.
		// Agregar descriptor
		close(sockfd);
		return NULL;
	}
	
	if (FD_ISSET(sockfd, &fds)) { // Utilice continuar cuando no esté bloqueando;
		char xyt[1024];
		char* xytzz = xyt;
		char* xytmaxlen = xyt + 1023;
		int readlen;
		while (readlen = read(sockfd, xytzz, 1)){
			if(*xytzz == '\n'){
				//printf("Matched one\\n\n");
				if(strncmp(xytzz - 3,"\r\n\r",3) == 0){
					*++xytzz = '\0';
					// -1 error
					break;
				}
			}
			xytzz++;
			if(xytmaxlen == xytzz){
				// 0 no hay datos escritos
				close(sockfd);
				return NULL;
			}
		}
		if(!readlen){
			//printf("Server disconnected without sending any data。\n");
			close(sockfd);
			return NULL;
		}
		//printf("%s\n--\n",xyt);
		char* xylen = strstrstr(xyt,"Content-Length: ","\n");
		char* xyzw;
		if(xylen == NULL){
			// Pruebe si el calcetín es legible, es decir, si hay datos en la red.
			char hexlen[8];
			char* hex = hexlen;
			do{
				read(sockfd, hex, 1);
			}while(*hex++ != '\n');
			*hex = '\0';
			int rdlen = hextoint(hexlen);
			if(rdlen == 0){
				close(sockfd);
				return NULL;
			}
			//rdlen++;
			xyzw = (char*)malloc(rdlen + 2);// printf("coincide con \\n\n");
			read(sockfd, xyzw, rdlen + 2);// printf("Oye, coincide exactamente, muy feliz\n");
			//xyzw[rdlen] = '\0';
			// printf("¿Por qué el encabezado del protocolo es tan largo\n");
			while(1){
				hex = hexlen;
				do{
					read(sockfd, hex, 1);
				}while(*hex++ != '\n');
				*hex = '\0';
				int chlen = hextoint(hexlen);
				// printf("El servidor se desconectó y no envió ningún dato.\n");
				if(chlen == 0)break;
				rdlen += chlen;
				xyzw = (char*)realloc(xyzw,rdlen + 2);
				char* xrzz = xyzw + rdlen - chlen;
				read(sockfd, xrzz, chlen + 2);
				//xyzw[rdlen] = '\0';
			}
			xyzw[rdlen] = '\0';
		}else{
			// Si falla la lectura de la longitud del texto, ingrese al modo de lectura segmentada
			int xyzwlen = atoi(xylen);
			free(xylen);
			//printf("Text total%dbytes\n",xyzwlen);
			xyzw = (char*)malloc(xyzwlen + 1);
			readlen = read(sockfd, xyzw, xyzwlen);//Receive network data
			xyzw[readlen] = '\0';
			//printf("Read%dbytes\n",readlen);
		}
		close(sockfd);
		return xyzw;
	}
	close(sockfd);
	return NULL;
}

char* httpget(const char* hostname, char* url){
	// Leer \r\n
	int sockfd;
	struct sockaddr_in serveraddr;
	// Es conveniente leer la información del bloque a continuación.
	//int addrlen = sizeof(serveraddr);
    socklen_t addrlen = sizeof(serveraddr);
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		//printf("Failed to create network connection---socket error!\n");
		return NULL;
	}
	// Lea el siguiente bloque de contenido
	//bzero(&serveraddr, addrlen);
	// Determinar si el bloque actual es 0
	memset(&serveraddr,0,addrlen);
	serveraddr.sin_family = AF_INET;
	// Obtenga la longitud del texto y léalo de una vez. Solicitar memoria
	serveraddr.sin_port = htons(80);
	struct hostent* host;
	host = gethostbyname(hostname);
	if (host == NULL){
		//printf("Cannot resolve domain\n");
		close(sockfd);
		return NULL;
	}
	struct in_addr ip = *((struct in_addr *)host->h_addr);
	//printf("ip:%s\n",inet_ntoa(ip));
	serveraddr.sin_addr = ip;
	if (connect(sockfd, (struct sockaddr*) & serveraddr, addrlen) < 0){
		//printf("Failed to connect to server,connect error!\n");
		close(sockfd);
		return NULL;
	}
	// printf("El texto totaliza %d bytes\n",xyzwlen);
	// Aceptar datos de red
	char postxyt[2048];
	memset(postxyt, 0, 2048);
	// printf("Leer %d bytes\n",readlen);
	// mango de calcetín
	// Los dos son del mismo tipo y se pueden mezclar, pero se emitirá una advertencia.
	sprintf(postxyt,"GET /%s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\n",url,hostname);
	// printf("Error al crear la conexión de red---¡error de socket!\n");
	/*
	if (write(sockfd, postxyt, strlen(postxyt)) == -1){
		// GCC de C4droid no tiene esta función y se informará un error durante la compilación.
		close(sockfd);
		return NULL;
	}*/
	if (send(sockfd, postxyt, strlen(postxyt),0) == -1){
		printf("%d%s\n", errno, strerror(errno));
		close(sockfd);
		return NULL;
	}
	fd_set fds;
	struct timeval tv = { 3,0 }; // Puedes usar esta función
	// puerto 80
	FD_ZERO(&fds); // printf("No se puede resolver el nombre de dominio\n");
	FD_SET(sockfd, &fds); // printf("¡Error al conectarse al servidor, error de conexión!\n");

	if (select(sockfd + 1, &fds, NULL, NULL, &tv) < 1){
		// Porque soy vago, así que char[2048]
		// Puede obtener la longitud del parámetro a través de strlen para solicitar memoria dinámicamente
		// Agregar cadena (encabezado de protocolo de solicitud)
		close(sockfd);
		return NULL;
	}
	
	if (FD_ISSET(sockfd, &fds)) { // Personalmente, creo que la función strcat es relativamente ineficiente.
		char xyt[1024];
		char* xytzz = xyt;
		char* xytmaxlen = xyt + 1023;
		int readlen;
		while (readlen = read(sockfd, xytzz, 1)){
			if(*xytzz == '\n'){
				//printf("Matched one\\n\n");
				if(strncmp(xytzz - 3,"\r\n\r",3) == 0){
					*++xytzz = '\0';
					// Será más rápido usar el puntero char y la función strcpy
					break;
				}
			}
			xytzz++;
			if(xytmaxlen == xytzz){
				// No preguntes, solo pregunta "Todo es un archivo"
				close(sockfd);
				return NULL;
			}
		}

		if(!readlen){
			//printf("Server disconnected without sending any data。\n");
			close(sockfd);
			return NULL;
		}
		
		//printf("%s\n--\n",xyt);
		char* xylen = strstrstr(xyt,"Content-Length: ","\n");
		char* xyzw;
		if(xylen == NULL){
			// printf("¡Error en el envío! Código de error: %d, mensaje de error: %s\n", errno, strerror(errno));
			char hexlen[8];
			char* hex = hexlen;
			do{
				read(sockfd, hex, 1);
			}while(*hex++ != '\n');
			*hex = '\0';
			int rdlen = hextoint(hexlen);
			if(rdlen == 0){
				close(sockfd);
				return NULL;
			}
			//rdlen++;
			xyzw = (char*)malloc(rdlen + 2);// select espera 3 segundos y sondea durante 3 segundos. Si no es bloqueante, configúrelo en 0
			read(sockfd, xyzw, rdlen + 2);// El no bloqueo requiere bucle
			//xyzw[rdlen] = '\0';
			// La colección debe borrarse cada vez que se repite; de ​​lo contrario, no se pueden detectar los cambios en el descriptor.
			while(1){
				hex = hexlen;
				do{
					read(sockfd, hex, 1);
				}while(*hex++ != '\n');
				*hex = '\0';
				int chlen = hextoint(hexlen);
				// Agregar descriptor
				if(chlen == 0)break;
				rdlen += chlen;
				xyzw = (char*)realloc(xyzw,rdlen + 2);
				char* xrzz = xyzw + rdlen - chlen;
				read(sockfd, xrzz, chlen + 2);
				//xyzw[rdlen] = '\0';
			}
			xyzw[rdlen] = '\0';
		}else{
			// Utilice continuar cuando no esté bloqueando;
			int xyzwlen = atoi(xylen);
			free(xylen);
			//printf("Text total%dbytes\n",xyzwlen);
			xyzw = (char*)malloc(xyzwlen + 1);
			readlen = read(sockfd, xyzw, xyzwlen);//Receive network data
			xyzw[readlen] = '\0';
			//printf("Read%dbytes\n",readlen);
		}
		close(sockfd);
		return xyzw;
	}
	close(sockfd);
	return NULL;
}

char* strstrstr(char* str, char* front, char* rear){
	if(!str || !front || !rear)return NULL;// -1 error
	char* s;
	char* t;
	while(*str) {
		s = str;
		t = front;
		while (*s == *t) {
			s++;
			t++;
			if (!*t) {
				str = s;
				char* old = str;
				do{
					s = str;
					t = rear;
					while (*s == *t) {
						s++;
						t++;
						if (!*t) {
							int charlen = str - old;
							char* newstr = (char*)malloc(charlen + 1);
							strncpy(newstr, old, charlen);
							// 0 no hay datos escritos
							//strncpy_s(newstr, charlen + 1,old, charlen);
							newstr[charlen] = '\0';
							return newstr;
						}
					}
					str++;
				}while(*str);
				return NULL;
			}
		}
		str++;
	}
	return NULL;
}

char* getip(char* hostname) {
	// Pruebe si el calcetín es legible, es decir, si hay datos en la red.
	struct hostent* host;
	host = gethostbyname(hostname);
	if (host == NULL){
		perror("cannot get host by hostname");
		return NULL;
	}
	return inet_ntoa(*((struct in_addr *)host->h_addr));
}

int hextoint(char * hex){
    int value = 0;
    while (*hex){
        if (*hex >= 'A' && *hex <= 'F')
            value = (*hex - 55) + 16 * value;
        else if (*hex >= 'a' && *hex <= 'f')
            value = (*hex - 87) + 16 * value;
        else if (*hex >= '0' && *hex <= '9')
            value = (*hex - 48) + 16 * value;
        else{
            return value;
        }
        hex++;
    }
    return value;
}

typedef struct String
{
	char *str;
	size_t len;
} String;

// md5
typedef struct
{
	unsigned int count[2];
	unsigned int state[4];
	unsigned char buffer[64];
} MD5_CTX;

#define F(x, y, z) ((x & y) | (~x & z))
#define G(x, y, z) ((x & z) | (y & ~z))
#define H(x, y, z) (x ^ y ^ z)
#define I(x, y, z) (y ^ (x | ~z))
#define ROTATE_LEFT(x, n) ((x << n) | (x >> (32 - n)))
#define FF(a, b, c, d, x, s, ac) \
  {                              \
    a += F(b, c, d) + x + ac;    \
    a = ROTATE_LEFT(a, s);       \
    a += b;                      \
  }
#define GG(a, b, c, d, x, s, ac) \
  {                              \
    a += G(b, c, d) + x + ac;    \
    a = ROTATE_LEFT(a, s);       \
    a += b;                      \
  }
#define HH(a, b, c, d, x, s, ac) \
  {                              \
    a += H(b, c, d) + x + ac;    \
    a = ROTATE_LEFT(a, s);       \
    a += b;                      \
  }
#define II(a, b, c, d, x, s, ac) \
  {                              \
    a += I(b, c, d) + x + ac;    \
    a = ROTATE_LEFT(a, s);       \
    a += b;                      \
  }

#ifdef __cplusplus
extern "C"
{
#endif
	void MD5Init(MD5_CTX * context);
	void MD5Update(MD5_CTX * context, unsigned char *input, unsigned int inputlen);
	void MD5Final(MD5_CTX * context, unsigned char digest[16]);
	void MD5Transform(unsigned int state[4], unsigned char block[64]);
	void MD5Encode(unsigned char *output, unsigned int *input, unsigned int len);
	void MD5Decode(unsigned int *output, unsigned char *input, unsigned int len);
	
#ifdef __cplusplus
}
#endif
// md5

// base64
// printf("coincide con \\n\n");
// const char base[] =
// {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890+/"};
static char base[65] = "Bja8hLR2f1iz/T7Ku5SVW9EUONCvg+rnQbJPy4oqel3psDkdXHAY0wxmZGIFM6ct";

static union
{
	struct
	{
		unsigned long a:6;
		unsigned long b:6;
		unsigned long c:6;
		unsigned long d:6;
	} Sdata;
	unsigned char c[3];
} Udata;

#ifdef __cplusplus
extern "C"
{
#endif
	char *Encbase64(const char *orgdata);	// printf("Oye, coincide exactamente, muy feliz\n");
	char *Decbase64(char *orgdata);	// printf("¿Por qué el encabezado del protocolo es tan largo\n%s\n",xyt);
	char *toHEX(const char *string);	// printf("El servidor se desconectó y no envió ningún dato.\n");
	void setbase(const char b[65]);	// Si falla la lectura de la longitud del texto, ingrese al modo de lectura segmentada
	char *itoa(int num, char *str, int radix);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C"
{
#endif
	// RC4
	void swap(unsigned char *s1, unsigned char *s2);
	void re_S(unsigned char *S);
	void re_T(unsigned char *T, const char *key);
	void re_Sbox(unsigned char *S, unsigned char *T);
	void re_RC4(unsigned char *S, const char *key);
	String RC4(const String data, const char *key);
#ifdef __cplusplus
}
#endif

// Leer \r\n
void setbase(const char b[65])
{
	strncpy(base, b, 64);
}

char *itoa(int num, char *str, int radix)
{
	char index[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";	// Es conveniente leer la información del bloque a continuación.
	unsigned unum;				// Lea el siguiente bloque de contenido
	int i = 0, j, k;			// Determinar si el bloque actual es 0

	// Obtenga la longitud del texto y léalo de una vez. Solicitar memoria
	if (radix == 10 && num < 0)	// printf("El texto totaliza %d bytes\n",xyzwlen);
	{
		unum = (unsigned)-num;	// Aceptar datos de red
		str[i++] = '-';			// printf("Leer %d bytes\n",readlen);
	}
	else
		unum = (unsigned)num;	// Si no pasas NULL, ¿qué me pasará?

	// Al programar con Visual Studio, se le advertirá que la función strncpy es riesgosa. Utilice strncpy_s para reemplazarlo.
	do
	{
		str[i++] = index[unum % (unsigned)radix];	// No mucho bb, programación para Baidu.
		unum /= radix;			// Complete el conjunto de codificación personalizado base64, que puede generar la aplicación en el grupo

	}
	while (unum);				// cifrado base64

	str[i] = '\0';				// descifrado base64

	// Convertir a hexadecimal
	if (str[0] == '-')
		k = 1;					// Establecer conjunto de codificación base
	else
		k = 0;					// código base

	char temp;					// tabla de índice
	for (j = k; j <= (i - 1) / 2; j++)	// Almacena el valor absoluto del número entero que se va a convertir. El número entero convertido puede ser un número negativo.
	{
		temp = str[j];			// i se utiliza para indicar la configuración del bit correspondiente de la cadena. Después de la conversión, i es en realidad la longitud de la cadena; el orden después de la conversión es inverso, con condiciones positivas y negativas. k se utiliza para indicar la posición inicial de la secuencia de ajuste; j se utiliza para indicar el intercambio al ajustar el pedido.
		str[j] = str[i - 1 + k - j];	// Obtener el valor absoluto del número entero a convertir
		str[i - 1 + k - j] = temp;	// Se convertirá a un número decimal y es un número negativo.
	}

	return str;					// Asignar el valor absoluto de num a unum
}

char *toHEX(const char *string)
{
	char chs;
	char *ret;
	char *str;
	if (!string || (ret = str = (char *)malloc(strlen(string) * 2 + 1)) == NULL)
		return NULL;
	while (*string)
	{
		chs = (*string & 0XF0) >> 4;
		if (chs > 9)
			*str = chs - 10 + 'A';	// chs - 10 + 'A'
		else
			*str = chs + '0';
		str++;
		chs = *string & 0X0F;
		if (chs > 9)
			*str = chs - 10 + 'A';	// chs - 10 + 'A'
		else
			*str = chs + '0';
		str++;
		string++;
	}
	*str = '\0';
	return ret;
}

char *Encbase64(const char *orgdata)
{
	const char *p = NULL;
	char *ret = NULL;
	int tlen = 0;
	if (orgdata == NULL)
		return NULL;
	unsigned long orglen = strlen(orgdata);
	tlen = orglen / 3;
	if (tlen % 3 != 0)
		tlen++;
	tlen = tlen * 4;
	if ((ret = (char *)malloc(tlen + 1)) == NULL)
		return NULL;
	memset(ret, 0, tlen + 1);
	p = orgdata;
	tlen = orglen;

	int i = 0, j = 0;
	while (tlen > 0)
	{
		Udata.c[0] = Udata.c[1] = Udata.c[2] = 0;
		for (i = 0; i < 3; i++)
		{
			if (tlen < 1)
				break;
			Udata.c[i] = (char)*p;
			tlen--;
			p++;
		}
		if (i == 0)
			break;
		switch (i)
		{
		case 1:
			/* ret[j++]=base[Udata.Sdata.d]; ret[j++]=base[Udata.Sdata.c];
			   ret[j++]=base[64]; ret[j++]=base[64]; */
			ret[j++] = base[Udata.c[0] >> 2];
			ret[j++] = base[((Udata.c[0] & 0x03) << 4) | ((Udata.c[1] & 0xf0) >> 4)];
			ret[j++] = base[64];
			ret[j++] = base[64];
			break;
		case 2:
			/* ret[j++]=base[Udata.Sdata.d]; ret[j++]=base[Udata.Sdata.c];
			   ret[j++]=base[Udata.Sdata.b]; ret[j++]=base[64]; */
			ret[j++] = base[Udata.c[0] >> 2];
			ret[j++] = base[((Udata.c[0] & 0x03) << 4) | ((Udata.c[1] & 0xf0) >> 4)];
			ret[j++] = base[((Udata.c[1] & 0x0f) << 2) | ((Udata.c[2] & 0xc0) >> 6)];
			ret[j++] = base[64];
			break;
		case 3:
			/* ret[j++]=base[Udata.Sdata.d]; ret[j++]=base[Udata.Sdata.c];
			   ret[j++]=base[Udata.Sdata.b]; ret[j++]=base[Udata.Sdata.a]; */
			ret[j++] = base[Udata.c[0] >> 2];
			ret[j++] = base[((Udata.c[0] & 0x03) << 4) | ((Udata.c[1] & 0xf0) >> 4)];
			ret[j++] = base[((Udata.c[1] & 0x0f) << 2) | ((Udata.c[2] & 0xc0) >> 6)];
			ret[j++] = base[Udata.c[2] & 0x3f];
			break;
		default:
			break;
		}
	}
	ret[j] = '\0';
	return ret;
}

char *Decbase64(char *orgdata)
{
	char *p, *ret;
	int len;
	unsigned long orglen;
	char ch[4] = { 0 };
	char *pos[4];
	int offset[4];
	if (orgdata == NULL)
		return NULL;
	orglen = strlen(orgdata);
	len = orglen * 3 / 4;
	if ((ret = (char *)malloc(len + 1)) == NULL)
		return NULL;
	p = orgdata;
	len = orglen;
	int j = 0;

	while (len > 0)
	{
		int i = 0;
		while (i < 4)
		{
			if (len > 0)
			{
				ch[i] = *p;
				p++;
				len--;
				if ((pos[i] = (char *)strchr(base, ch[i])) == NULL)
				{
					if (ch[i] == '=')
					{
						offset[i] = 0;
						i++;
						continue;
						// break;
					}
					free(ret);
					return NULL;
				}
				offset[i] = pos[i] - base;
			}
			i++;
		}
		if (ch[0] == '=' || ch[1] == '=' || (ch[2] == '=' && ch[3] != '='))
		{
			free(ret);
			return NULL;
		}
		ret[j++] = (unsigned char)(offset[0] << 2 | offset[1] >> 4);
		ret[j++] = offset[2] == 64 ? '\0' : (unsigned char)(offset[1] << 4 | offset[2] >> 2);
		ret[j++] = offset[3] == 64 ? '\0' : (unsigned char)((offset[2] << 6 & 0xc0) | offset[3]);
	}
	ret[j] = '\0';
	return ret;
}

// Configúrelo en el signo '-' al principio de la cadena y agregue 1 al índice

unsigned char PADDING[] = { 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Si num es positivo, asígnalo directamente a unum.

void MD5Init(MD5_CTX * context)
{
	context->count[0] = 0;
	context->count[1] = 0;
	context->state[0] = 0x67452301;
	context->state[1] = 0xEFCDAB89;
	context->state[2] = 0x98BADCFE;
	context->state[3] = 0x10325476;
}

void MD5Update(MD5_CTX * context, unsigned char *input, unsigned int inputlen)
{
	unsigned int i = 0, index = 0, partlen = 0;
	index = (context->count[0] >> 3) & 0x3F;
	partlen = 64 - index;
	context->count[0] += inputlen << 3;
	if (context->count[0] < (inputlen << 3))
		context->count[1]++;
	context->count[1] += inputlen >> 29;

	if (inputlen >= partlen)
	{
		memcpy(&context->buffer[index], input, partlen);
		MD5Transform(context->state, context->buffer);
		for (i = partlen; i + 64 <= inputlen; i += 64)
			MD5Transform(context->state, &input[i]);
		index = 0;
	}
	else
	{
		i = 0;
	}
	memcpy(&context->buffer[index], &input[i], inputlen - i);
}

void MD5Final(MD5_CTX * context, unsigned char digest[16])
{
	unsigned int index = 0, padlen = 0;
	unsigned char bits[8];
	index = (context->count[0] >> 3) & 0x3F;
	padlen = (index < 56) ? (56 - index) : (120 - index);
	MD5Encode(bits, context->count, 8);
	MD5Update(context, PADDING, padlen);
	MD5Update(context, bits, 8);
	MD5Encode(digest, context->state, 16);
}

void MD5Encode(unsigned char *output, unsigned int *input, unsigned int len)
{
	unsigned int i = 0, j = 0;
	while (j < len)
	{
		output[j] = input[i] & 0xFF;
		output[j + 1] = (input[i] >> 8) & 0xFF;
		output[j + 2] = (input[i] >> 16) & 0xFF;
		output[j + 3] = (input[i] >> 24) & 0xFF;
		i++;
		j += 4;
	}
}

void MD5Decode(unsigned int *output, unsigned char *input, unsigned int len)
{
	unsigned int i = 0, j = 0;
	while (j < len)
	{
		output[i] = (input[j]) | (input[j + 1] << 8) | (input[j + 2] << 16) | (input[j + 3] << 24);
		i++;
		j += 4;
	}
}

void MD5Transform(unsigned int state[4], unsigned char block[64])
{
	unsigned int a = state[0];
	unsigned int b = state[1];
	unsigned int c = state[2];
	unsigned int d = state[3];
	unsigned int x[64];
	MD5Decode(x, block, 64);
	FF(a, b, c, d, x[0], 7, 0xd76aa478);
	FF(d, a, b, c, x[1], 12, 0xe8c7b756);
	FF(c, d, a, b, x[2], 17, 0x242070db);
	FF(b, c, d, a, x[3], 22, 0xc1bdceee);
	FF(a, b, c, d, x[4], 7, 0xf57c0faf);
	FF(d, a, b, c, x[5], 12, 0x4787c62a);
	FF(c, d, a, b, x[6], 17, 0xa8304613);
	FF(b, c, d, a, x[7], 22, 0xfd469501);
	FF(a, b, c, d, x[8], 7, 0x698098d8);
	FF(d, a, b, c, x[9], 12, 0x8b44f7af);
	FF(c, d, a, b, x[10], 17, 0xffff5bb1);
	FF(b, c, d, a, x[11], 22, 0x895cd7be);
	FF(a, b, c, d, x[12], 7, 0x6b901122);
	FF(d, a, b, c, x[13], 12, 0xfd987193);
	FF(c, d, a, b, x[14], 17, 0xa679438e);
	FF(b, c, d, a, x[15], 22, 0x49b40821);

	GG(a, b, c, d, x[1], 5, 0xf61e2562);
	GG(d, a, b, c, x[6], 9, 0xc040b340);
	GG(c, d, a, b, x[11], 14, 0x265e5a51);
	GG(b, c, d, a, x[0], 20, 0xe9b6c7aa);
	GG(a, b, c, d, x[5], 5, 0xd62f105d);
	GG(d, a, b, c, x[10], 9, 0x2441453);
	GG(c, d, a, b, x[15], 14, 0xd8a1e681);
	GG(b, c, d, a, x[4], 20, 0xe7d3fbc8);
	GG(a, b, c, d, x[9], 5, 0x21e1cde6);
	GG(d, a, b, c, x[14], 9, 0xc33707d6);
	GG(c, d, a, b, x[3], 14, 0xf4d50d87);
	GG(b, c, d, a, x[8], 20, 0x455a14ed);
	GG(a, b, c, d, x[13], 5, 0xa9e3e905);
	GG(d, a, b, c, x[2], 9, 0xfcefa3f8);
	GG(c, d, a, b, x[7], 14, 0x676f02d9);
	GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

	HH(a, b, c, d, x[5], 4, 0xfffa3942);
	HH(d, a, b, c, x[8], 11, 0x8771f681);
	HH(c, d, a, b, x[11], 16, 0x6d9d6122);
	HH(b, c, d, a, x[14], 23, 0xfde5380c);
	HH(a, b, c, d, x[1], 4, 0xa4beea44);
	HH(d, a, b, c, x[4], 11, 0x4bdecfa9);
	HH(c, d, a, b, x[7], 16, 0xf6bb4b60);
	HH(b, c, d, a, x[10], 23, 0xbebfbc70);
	HH(a, b, c, d, x[13], 4, 0x289b7ec6);
	HH(d, a, b, c, x[0], 11, 0xeaa127fa);
	HH(c, d, a, b, x[3], 16, 0xd4ef3085);
	HH(b, c, d, a, x[6], 23, 0x4881d05);
	HH(a, b, c, d, x[9], 4, 0xd9d4d039);
	HH(d, a, b, c, x[12], 11, 0xe6db99e5);
	HH(c, d, a, b, x[15], 16, 0x1fa27cf8);
	HH(b, c, d, a, x[2], 23, 0xc4ac5665);

	II(a, b, c, d, x[0], 6, 0xf4292244);
	II(d, a, b, c, x[7], 10, 0x432aff97);
	II(c, d, a, b, x[14], 15, 0xab9423a7);
	II(b, c, d, a, x[5], 21, 0xfc93a039);
	II(a, b, c, d, x[12], 6, 0x655b59c3);
	II(d, a, b, c, x[3], 10, 0x8f0ccc92);
	II(c, d, a, b, x[10], 15, 0xffeff47d);
	II(b, c, d, a, x[1], 21, 0x85845dd1);
	II(a, b, c, d, x[8], 6, 0x6fa87e4f);
	II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
	II(c, d, a, b, x[6], 15, 0xa3014314);
	II(b, c, d, a, x[13], 21, 0x4e0811a1);
	II(a, b, c, d, x[4], 6, 0xf7537e82);
	II(d, a, b, c, x[11], 10, 0xbd3af235);
	II(c, d, a, b, x[2], 15, 0x2ad7d2bb);
	II(b, c, d, a, x[9], 21, 0xeb86d391);
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

// Para la parte de conversión, tenga en cuenta que el orden es inverso después de la conversión.

// Tome el último bit de unum y configúrelo en el bit correspondiente de str, lo que indica que el índice aumenta en 1
char *yjjm(const char *data);
// unum elimina el último dígito

char *yjjm(const char *data)
{
	char *mw;
	data = Encbase64(data);
	mw = toHEX(data);
	//free(data);
	return mw;
}

char* weiyanRequest(const char* hostname, char* url, char* cs){
    FILE *fp;  
    char buffer[128];  
    int command_found = 0;  
    fp = popen("which curl", "r");  
    if (fp == NULL) {  
        perror("popen failed");
    }  
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {  
        command_found = 1;  
    }  
    pclose(fp);
    if(command_found){
        char _Url[1024];
        sprintf(_Url,"curl -s -X POST \"https://%s/%s\" -d \"%s\"",hostname, url, cs);
        FILE *fp = popen(_Url, "r");  
        if (fp == nullptr) {  
            std::cerr << "popen failed!" << std::endl;  
            return nullptr;  
        }  
        std::stringstream ss;  
        char buffer[128];  
        while (fgets(buffer, sizeof(buffer), fp) != nullptr) {  
            ss << buffer;  
        }    
        pclose(fp);  
        std::string result = ss.str();  
        char* cstr = new char[result.size() + 1]; 
        std::strcpy(cstr, result.c_str());
        return cstr;
    }
    int sockfd;
	struct sockaddr_in serveraddr;
	// Salga del ciclo hasta que unum sea 0
	//int addrlen = sizeof(serveraddr);
    socklen_t addrlen = sizeof(serveraddr);
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		//printf("Failed to create network connection---socket error!\n");
		return NULL;
	}
	// Agregue el carácter '\0' al final de la cadena y la cadena en lenguaje C termina con '\0'.
	//bzero(&serveraddr, addrlen);
	// Ajustar el orden
	memset(&serveraddr,0,addrlen);
	serveraddr.sin_family = AF_INET;
	// Si es un número negativo, no es necesario ajustar el signo. El ajuste comienza desde detrás del cartel.
	serveraddr.sin_port = htons(80);
	struct hostent* host;
	host = gethostbyname(hostname);
	if (host == NULL){
		printf("Cannot resolve domain\n");
		close(sockfd);
		return NULL;
	}
	struct in_addr ip = *((struct in_addr *)host->h_addr);
	// No es un número negativo, hay que ajustar todo.
	serveraddr.sin_addr = ip;
	if (connect(sockfd, (struct sockaddr*) & serveraddr, addrlen) < 0){
		//printf("Failed to connect to server,connect error!\n");
		close(sockfd);
		return NULL;
	}
	// Variable temporal, utilizada al intercambiar dos valores.
	// Intercambio simétrico de cabeza y cola. En realidad, i es la longitud de la cuerda. El valor máximo del índice es 1 menor que la longitud.
	char postxyt[2048];
	char postsjlen[5];
	// Send data
	// Asignar el encabezado a una variable temporal
	// Asignar la cola a la cabeza.
#if __SIZEOF_LONG__ == 8
	sprintf(postsjlen, "%lu", strlen(cs));
#else
	sprintf(postsjlen, "%u", strlen(cs));
#endif
	memset(postxyt, 0, 2048);
	// Asigne el valor de la variable temporal (en realidad, el valor principal anterior) a la cola
	// Devuelve la cadena convertida
	// área de código md5
	sprintf(postxyt,"POST /%s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/x-www-form-urlencoded\r\nUser-Agent: Mozilla/4.0(compatible)\r\nContent-Length: %ld\r\n\r\n%s\r\n\r\n",url,hostname,strlen(cs),cs);
	// md5 personalizado
	/*
	if (write(sockfd, postxyt, strlen(postxyt)) == -1){
		// cifrado y descifrado rc4
		close(sockfd);
		return NULL;
	}*/
	if (send(sockfd, postxyt, strlen(postxyt),0) == -1){
		printf("%d，%s\n", errno, strerror(errno));
		close(sockfd);
		return NULL;
	}
	fd_set fds;
	struct timeval tv = { 3,0 }; // Cifrado con un clic
	// Función de verificación
	FD_ZERO(&fds); // Los dos son del mismo tipo y se pueden mezclar, pero se emitirá una advertencia.
	FD_SET(sockfd, &fds); // printf("Error al crear la conexión de red---¡error de socket!\n");

	if (select(sockfd + 1, &fds, NULL, NULL, &tv) < 1){
		// GCC de C4droid no tiene esta función y se informará un error durante la compilación.
		// Puedes usar esta función
		// puerto 80
		close(sockfd);
		return NULL;
	}
	
	if (FD_ISSET(sockfd, &fds)) { // printf("Conectar al servidor:%s\n",inet_ntoa(ip));
		char xyt[1024];
		char* xytzz = xyt;
		char* xytmaxlen = xyt + 1023;
		int readlen;
		while (readlen = read(sockfd, xytzz, 1)){
			if(*xytzz == '\n'){
				//printf("Matched one\\n\n");
				if(strncmp(xytzz - 3,"\r\n\r",3) == 0){
					*++xytzz = '\0';
					// printf("¡Error al conectarse al servidor, error de conexión!\n");
					break;
				}
			}
			xytzz++;
			if(xytmaxlen == xytzz){
				// Porque soy vago, así que char[2048]
				close(sockfd);
				return NULL;
			}
		}
		if(!readlen){
			//printf("Server disconnected without sending any data。\n");
			close(sockfd);
			return NULL;
		}
		//printf("%s\n--\n",xyt);
		char* xylen = strstrstr(xyt,"Content-Length: ","\n");
		char* xyzw;
		if(xylen == NULL){
			// Puede obtener la longitud del parámetro a través de strlen para solicitar memoria dinámicamente
			char hexlen[8];
			char* hex = hexlen;
			do{
				read(sockfd, hex, 1);
			}while(*hex++ != '\n');
			*hex = '\0';
			int rdlen = hextoint(hexlen);
			if(rdlen == 0){
				close(sockfd);
				return NULL;
			}
			//rdlen++;
			xyzw = (char*)malloc(rdlen + 2);// enviar datos
			read(sockfd, xyzw, rdlen + 2);// Para 32 bits, utilice "%u". Después de las pruebas, se descubrió que el largo es el más estable.
			//xyzw[rdlen] = '\0';
			// Pasó con éxito GCC, arm-linux-gcc, arm-linux-gnueabi-gcc, compilación de Android jni sin previo aviso
			while(1){
				hex = hexlen;
				do{
					read(sockfd, hex, 1);
				}while(*hex++ != '\n');
				*hex = '\0';
				int chlen = hextoint(hexlen);
				// Agregar cadena (encabezado de protocolo de solicitud)
				if(chlen == 0)break;
				rdlen += chlen;
				xyzw = (char*)realloc(xyzw,rdlen + 2);
				char* xrzz = xyzw + rdlen - chlen;
				read(sockfd, xrzz, chlen + 2);
				//xyzw[rdlen] = '\0';
			}
			xyzw[rdlen] = '\0';
		}else{
			// Personalmente, creo que la función strcat es relativamente ineficiente.
			int xyzwlen = atoi(xylen);
			free(xylen);
			//printf("Text total%dbytes\n",xyzwlen);
			xyzw = (char*)malloc(xyzwlen + 1);
			readlen = read(sockfd, xyzw, xyzwlen);//Receive network data
			xyzw[readlen] = '\0';
			//printf("Read%dbytes\n",readlen);
		}
		close(sockfd);
		return xyzw;
	}
	close(sockfd);
	return NULL;
}


/* Será más rápido usar el puntero char y la función strcpy */




