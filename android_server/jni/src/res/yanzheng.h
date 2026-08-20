#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <fstream>
#include <string.h>
#include <time.h>
#include <malloc.h>
#include <iostream>
#include <fstream>
#include "weiyan.h"
#include "cJSON.h"
#include "cJSON.c"
#include "Encrypt.h"
#include<iostream>
#include<ctime>


using namespace std;

int Touchdriver()
{
    return 0;
    char *host = "wy.llua.cn";
    // Clave

    char *APPID = "58631";
    // Ingresar APPID
    
    const static char *APPKEY = "tQn1Q749XEx4XoEx";
    // Ingresar APPKEY

    const static char *RC4KEY = "GCOI155CpxX58631";
    // Panel de usuario - Clave RC4

    const static char *km_luj = "/sdcard/km";
    // Clave

    const static char *imei_luj = "/sdcard/imei";
    // Ruta del codigo de maquina

    printf("\033[35;1m");       // Color rosa
    printf("Executed Victor\n");
    printf("\033[32;1m");       // GreenColor
 
    
    home_main:
    char km[128];                // Color verde
    if (fopen(km_luj, "r") == NULL)
    {
        printf("\033[31;1m");
        printf("[-] Por favor, introduzca la clave:");
        char str[] = "";
        scanf("%s",&str);
    
        FILE *fp = fopen(km_luj, "w");
        if (fp != NULL) {
            fprintf(fp, "%s", str);
            fclose(fp);
        }
        std::cout << "[-] ¡Escritura exitosa! Re-verificando clave" << std::endl;
    }
    fscanf(fopen(km_luj, "r"), "%s", &km);


    char imei[512];              // Clave
    if (fopen(imei_luj, "r") == NULL)
    {

        printf("\033[31;1m");
        printf("[-] Error al obtener codigo de dispositivo\n");
        srand(time(NULL)); // Codigo de dispositivo
        char* str = (char*)malloc((20 + 1) * sizeof(char));
        int i;
        for (i = 0; i < 20; i++) {
            int randomNum = rand() % 26; // Establecer semilla random a tiempo actual
            str[i] = 'a' + randomNum; // Generar random 0-25
        }
        str[20] = '\0'; // Convertir random a letra minúscula
    
        FILE *fp = fopen(imei_luj, "w");
        if (fp == NULL) {
            printf("[-] Error al crear archivo");
            return 1;
        }
        fprintf(fp, "%s", str);
    
        fclose(fp);
    
        std::cout << "[-] ¡Codigo obtenido! Re-verificando clave" << std::endl;
 
        
    }
    fscanf(fopen(imei_luj, "r"), "%s", &imei);


    printf("[-] Clave:%s\n[-] Codigo de disp:%s\n", km, imei);
    // ---------------------------------------------------------

    if (km == "" or imei == "")
    {
        printf("\033[31;1m");
        printf("[-] Falta codigo de dispositivo o clave");
        exit(1);
    }

    // Añadir terminador nulo
    time_t t;
    t = time(NULL);
    int ii = time(&t);
    srand(time(NULL));
    // Marca de tiempo
    char value[512];
    char sign[1024];
    char data[256];
    sprintf(value, "%d%d", ii,rand());
    sprintf(sign, "kami=%s&markcode=%s&t=%d&%s", km, imei, ii, APPKEY);


    // ---------------------------------------------------------
    // Combinar datos
    char *aaa = sign;
    unsigned char *bbb = (unsigned char *)aaa;
    MD5_CTX md5c;
    MD5Init(&md5c);
    int i;
    unsigned char decrypt[16];
    MD5Update(&md5c, bbb, strlen((char *)bbb));
    MD5Final(&md5c, decrypt);
    char lkey[512] = { 0 };
    for (i = 0; i < 16; i++)
    {
        sprintf(&lkey[i * 2], "%02x", decrypt[i]);
    }
    // Validar firma MD5
    // ---------------------------------------------------------

    // Validar firma MD5
    sprintf(data, "kami=%s&markcode=%s&t=%d&sign=%s&value=%s", km, imei, ii, lkey, value);
    char *dataa=Encrypt(data, RC4KEY);

    // Encriptacion RC4
    char cs[512];
    sprintf(cs, "&data=%s", dataa);
    
    char url[512];
    sprintf(url, "api/?id=kmlogon&app=%s",APPID);
    
    // Combinar datos
    char *tijiao = httppost(host,url,cs);

    // Enviar datos
    char* tijiaoo=Decrypt(tijiao, RC4KEY);
    
    // Desencriptacion RC4
    cJSON *cjson = cJSON_Parse(tijiaoo);
    
    // Analizar JSON
    int code = cJSON_GetObjectItem(cjson, "code")->valueint;

    // Leer codigo de estado
    int time = cJSON_GetObjectItem(cjson, "time")->valueint;

    // Tiempo del servidor
    char *msg = cJSON_GetObjectItem(cjson, "msg")->valuestring;
    
    // Mensaje de error
    char *check = cJSON_GetObjectItem(cjson, "check")->valuestring;

    // Validacion de inicio de sesion
    if (code == 200) // Verificar inicio de sesion
    {
        cJSON *msgdata = cJSON_GetObjectItem(cjson, "msg");

        // Comprobar codigo
        long vip = cJSON_GetObjectItem(msgdata, "vip")->valuedouble;

        char weijy[512];
        sprintf(weijy, "%d%s%s", time, APPKEY, value);

        // ---------------------------------------------------------
        // Marca de tiempo
        char *aaaa = weijy;
        unsigned char *bbbb = (unsigned char *)aaaa;
        MD5_CTX md5c;
        MD5Init(&md5c);
        int i;
        unsigned char decrypt[16*16];
        MD5Update(&md5c, bbbb, strlen((char *)bbbb));
        MD5Final(&md5c, decrypt);
        char ykey[256] = { 0 };
        for (i = 0; i < 16; i++)
        {
            sprintf(&ykey[i * 2], "%02x", decrypt[i]);
        }
        // Validar firma MD5
        // ---------------------------------------------------------
        if (string(ykey) == check)
        {
            printf("\033[32;1m");   // GreenColor
            printf("[-] Inicio de sesion exitoso\n");
            if (vip)
            {
                char vipmsg[256];
                sprintf(vipmsg, "%ld", vip);
                time_t timestamp = std::atoll(vipmsg);  // Validar firma MD5
                std::tm * timeinfo = std::localtime(&timestamp);    // Color verde
                char buffer[512];
                std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);   // Marca de tiempo
                std::cout << "[-] Tiempo de caducidad: " << buffer << std::endl;
            }
        }
        else
        {
            printf("[-] Validacion fallida\n");
            remove(km_luj);
            goto home_main;
        }
    }
    else
    {
        printf("\033[35;1m");   // Marca de tiempo
        cout << msg << endl;
        remove(km_luj);
        goto home_main;
    }
    return 0;
}
// Formatear tiempo

