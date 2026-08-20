#pragma once
#include <dirent.h>
#include <stdio.h>
#include <string>
#include <array>
#include <map>
#include <cmath>
#include "Driver.h"
#define PI 3.141592653589793238
ImColor PlayerColor = ImColor(0,255,0,255);
ImColor AIColor = ImColor(255, 255, 255, 255);
ImColor HalflackColor = ImColor(ImVec4(0/255.f, 0/255.f, 0/255.f, 0.3f));
bool Movementh;
ImColor RandomColor(){
    int R, G, B, A = 140;
    R = (random() % 255);
    G = (random() % 255);
    B = (random() % 255);
    return ImColor(R, G, B, A);
}
long Countt;
ImColor ColorArr[100];
void ColorInitialization(){
    for(int i = 0; i < 100; i++) {
        ColorArr[i] = RandomColor();
    }
}

ImColor TeamColor(int Num){
    if(Num < 99 && Num > 0) {
        return ColorArr[Num];
    } else {
        return ImColor(67, 205, 128, 150);
    }
}

pid_t ProcessID;
uint64_t ModuleAddr;
uint64_t GName, WorldMatrix,MatrixBase,Uworld;
uint64_t WorldAddr;
uint64_t ArrayObj;
uint64_t WorldArray;
int WorldCount;
int bIsShooting;
uint64_t libbase,Matrix;
float px,py;
int VersionNum;
const char* getLSXKernelVersionName;
unsigned long getExpireDate;
bool Norecoil;
bool BulletFocus;
float TouchPosx = 1890;
int screenX = 0, screenY = 0;

float screen_x = 0 ,screen_y = 0;
float TouchPosy = 682;
float NumIo[50];
bool DrawIo[50];
float MatrixArray[16] = {0};
int Count;
float matrix[16] = { 0 }, angle, camera, r_x, r_y, r_w;
int CoordSwitch  = 0;
int BoneList[][2]
{
	{15, 82},// Clavicula Cuello
	{15, 1},// Clavicula Pelvis
	{15, 53},// Clavicula Hombro izquierdo
	{53, 54},// Hombro izquierdo Codo izquierdo
	{54, 87},// Codo izquierdo Muñeca izquierda
	{15, 23},// Clavicula Hombro derecho
	{23, 24},// Hombro derecho Codo derecho
	{24, 86},// Codo derecho Muñeca derecha
	{1, 2},// Ano Nalgas izquierdas
	{2, 4},// Nalgas izquierdas Rodilla izquierda
	{4, 92},// Rodilla izquierda Talon izquierdo
	{1, 7},// Ano Nalgas derechas
	{7, 9},// Nalgas derechas Rodilla derecha
	{9, 94},// Rodilla derecha Talon derecho
};
inline int AB_BoneList[][2]
{
	{15, 82},// Clavicula Cuello
	{15, 1},// Clavicula Pelvis
	{15, 53},// Clavicula Hombro izquierdo
	{53, 54},// Hombro izquierdo Codo izquierdo
	{54, 87},// Codo izquierdo Muñeca izquierda
	{15, 23},// Clavicula Hombro derecho
	{23, 24},// Hombro derecho Codo derecho
	{24, 86},// Codo derecho Muñeca derecha
	{1, 2},// Ano Nalgas izquierdas
	{2, 4},// Nalgas izquierdas Rodilla izquierda
	{4, 92},// Rodilla izquierda Talon izquierdo
	{1, 7},// Ano Nalgas derechas
	{7, 9},// Nalgas derechas Rodilla derecha
	{9, 94},// Rodilla derecha Talon derecho
};





ImColor LevelColor1 = ImColor(255, 255, 255, 255);// Botin nivel 1
ImColor LevelColor2 = ImColor(128, 0, 128, 255);// Botin nivel 2
ImColor LevelColor3 = ImColor(255, 0, 0, 255);// Botin nivel 3
ImColor Yellow = ImColor(255,255,0);
ImColor Black = ImColor(0,0,0);
ImColor Green = ImColor(0,255,0);
ImColor Blue = ImColor(0,0,255);
ImColor White = ImColor(255,250,255);
ImColor Red = ImColor(255, 0, 0);
ImColor CoRed = ImColor(255, 0, 0, 255);// Botin nivel 3
inline ImColor LightBlue = ImColor(ImVec4(36 / 255.f, 249 / 255.f, 217 / 255.f, 255 / 255.f));
inline ImColor LightPink = ImColor(ImVec4(255 / 255.f, 200 / 255.f, 250 / 255.f, 0.95f));
inline ImColor HalfBlack = ImColor(ImVec4(0 / 255.f, 0 / 255.f, 0 / 255.f, 0.18f));
inline ImColor BloodColor = ImColor(ImVec4(0 / 255.f, 249 / 255.f, 0 / 255.f, 0.35f));
inline ImColor Orange = ImColor(ImVec4(255 / 255.f, 150 / 255.f, 30 / 255.f, 0.95f));
inline ImColor Pink = ImColor(ImVec4(220 / 255.f, 108 / 255.f, 1202 / 255.f, 0.95f));
inline ImColor Purple = ImColor(ImVec4(169 / 255.f, 120 / 255.f, 223 / 255.f, 0.95f));
inline ImColor Blank = ImColor(ImVec4(1.0 / 255.f, 1.0 / 255.f, 1.0 / 255.f, 0.0f));

int readcount(int *c, int Num) {
        ++*c;
        return Num;
    }
typedef unsigned short UTF16;
typedef char UTF8;
void GetUTF8Text(UTF8* buf, uint64_t namepy)
{
	buf[0] = '\0';
	if (namepy < 0x1000000000ULL) return;
	UTF16 buf16[16] = { 0 };
	if (!driver->read(namepy, buf16, 28)) return;
	UTF16* pTempUTF16 = buf16;
	UTF8* pTempUTF8 = buf;
	UTF8* pUTF8End = pTempUTF8 + 31;
	while (pTempUTF16 < buf16 + 16 && *pTempUTF16 != 0)
	{
		uint32_t codepoint = 0;
		if (*pTempUTF16 >= 0xD800 && *pTempUTF16 <= 0xDBFF)
		{
			if (pTempUTF16 + 1 < buf16 + 16 && *(pTempUTF16 + 1) >= 0xDC00 && *(pTempUTF16 + 1) <= 0xDFFF)
			{
				codepoint = ((*(pTempUTF16++) - 0xD800) << 10) + (*(pTempUTF16)-0xDC00) + 0x10000;
			}
			else break;
		}
		else codepoint = *pTempUTF16;

		if (codepoint >= 0x20 && codepoint <= 0x7E && pTempUTF8 + 1 < pUTF8End)
		{
			*pTempUTF8++ = (UTF8)codepoint;
		}
		else if (codepoint >= 0x0080 && codepoint <= 0x07FF && pTempUTF8 + 2 < pUTF8End)
		{
			*pTempUTF8++ = (codepoint >> 6) | 0xC0;
			*pTempUTF8++ = (codepoint & 0x3F) | 0x80;
		}

		pTempUTF16++;
	}
}

struct Vector2A
{
	float X;
	float Y;

	Vector2A()
	{
		this->X = 0;
		this->Y = 0;
	}

	Vector2A(float x, float y)
	{
		this->X = x;
		this->Y = y;
	}

	// Definir operador <, ajustar reglas de comparacion segun necesidad
	bool operator<(const Vector2A& other) const
	{
		if (X < other.X)
			return true;
		else if (X == other.X && Y < other.Y)
			return true;
		else
			return false;
	}
};

struct Vector3A {
    float X;
    float Y;
    float Z;

    inline Vector3A() : X(0), Y(0), Z(0) {}

    inline Vector3A(float X, float Y, float Z) : X(X), Y(Y), Z(Z) {}

    //inline  eXplicit Vector3A(float value) : X(value), Y(value), Z(value) {}

    inline Vector3A operator+(const Vector3A &other) const {
        return Vector3A(X + other.X, Y + other.Y, Z + other.Z);
    }

    inline Vector3A operator+(const float other) const {
        return Vector3A(X + other, Y + other, Z + other);
    }

    inline Vector3A operator-(const Vector3A &other) const {
        return Vector3A(X - other.X, Y - other.Y, Z - other.Z);
    }

    inline Vector3A operator-(const float other) const {
        return Vector3A(X - other, Y - other, Z - other);
    }

    inline Vector3A operator*(const Vector3A &other) const {
        return Vector3A(X * other.X, Y * other.Y, Z * other.Z);
    }

    inline Vector3A operator*(const float value) const {
        return Vector3A(X * value, Y * value, Z * value);
    }

    inline Vector3A operator/(const float value) const {
        if (value != 0) {
            return Vector3A(X / value, Y / value, Z / value);
        }
        return Vector3A();
    }

    inline Vector3A operator-() const {
        return Vector3A(-X, -Y, -Z);
    }

    inline Vector3A &operator+=(const Vector3A &other) {
        X += other.X;
        Y += other.Y;
        Z += other.Z;
        return *this;
    }

    inline Vector3A &operator-=(const Vector3A &other) {
        X -= other.X;
        Y -= other.Y;
        Z -= other.Z;
        return *this;
    }

    inline Vector3A &operator+=(const float value) {
        X += value;
        Y += value;
        Z += value;
        return *this;
    }

    inline Vector3A &operator-=(const float value) {
        X -= value;
        Y -= value;
        Z -= value;
        return *this;
    }

    inline Vector3A &operator*=(const float value) {
        X *= value;
        Y *= value;
        Z *= value;
        return *this;
    }

    inline Vector3A &operator*=(const Vector3A &other) {
        X *= other.X;
        Y *= other.Y;
        Z *= other.Z;
        return *this;
    }

    inline Vector3A &operator/=(const float &value) {
        X /= value;
        Y /= value;
        Z /= value;
        return *this;
    }

    inline Vector3A &operator=(const Vector3A &other) {
        X = other.X;
        Y = other.Y;
        Z = other.Z;
        return *this;
    }

    inline bool operator==(const Vector3A &other) const {
        return X == other.X && Y == other.Y && Z == other.Z;
    }

    inline bool operator!=(const Vector3A &other) const {
        return X != other.X || Y != other.Y || Z != other.Z;
    }

    inline float operator[](int indeX) const {
        return (&X)[indeX];
    }

    inline float &operator[](int indeX) {
        return (&X)[indeX];
    }

    inline void Zero() {
        X = Y = Z = 0;
    }

    inline float length() const {
        return sqrt(X * X + Y * Y + Z * Z);
    }

    inline bool isValid() const {
        return X != 0 && Y != 0 && Z != 0 && !isnan(X) && !isnan(Y) && !isnan(Z);
    }

    static inline float dot(const Vector3A &a, const Vector3A &b) {
        return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
    }

    static inline bool inRange(const Vector3A &target, const Vector3A &min, const Vector3A &max) {
        return target.X > min.X && target.X < max.X && target.Y > min.Y && target.Y < max.Y &&
               target.Z > min.Z && target.Z < max.Z;
    }
};




struct D2DVector
{
	float X;
	float Y;
};

struct D3DVector
{
	float X;
	float Y;
	float Z;
};

Vector3A Pos;
struct FMatrix
{
	float M[4][4];
};

class FRotator
{
public:
    FRotator() :Pitch(0.f), Yaw(0.f), Roll(0.f) {

    }
    FRotator(float _Pitch, float _Yaw, float _Roll) : Pitch(_Pitch), Yaw(_Yaw), Roll(_Roll)
    {

    }
    ~FRotator()
    {

    }
    float Pitch;
    float Yaw;
    float Roll;
    inline FRotator Clamp()
    {

        if (Pitch > 180)
        {
            Pitch -= 360;
        }
        else
        {
            if (Pitch < -180)
            {
                Pitch += 360;
            }
        }
        if (Yaw > 180)
        {
            Yaw -= 360;
        }
        else {
            if (Yaw < -180)
            {
                Yaw += 360;
            }
        }
        if (Pitch > 89)
        {
            Pitch = 89;
        }
        if (Pitch < -89)
        {
            Pitch = -89;
        }
        while (Yaw < 180)
        {
            Yaw += 360;
        }
        while (Yaw > 180)
        {
            Yaw -= 360;
        }
        Roll = 0;
        return FRotator(Pitch, Yaw, Roll);
    }
    inline float Length()
    {
        return sqrtf(Pitch * Pitch + Yaw * Yaw + Roll * Roll);
    }
    FRotator operator+(FRotator v) {
        return FRotator(Pitch + v.Pitch, Yaw + v.Yaw, Roll + v.Roll);
    }
    FRotator operator-(FRotator v) {
        return FRotator(Pitch - v.Pitch, Yaw - v.Yaw, Roll - v.Roll);
    }
};

struct Quat
{
	float X;
	float Y;
	float Z;
	float W;
};


struct FTransform
{
	Quat Rotation;
	Vector3A Translation;
	Vector3A Scale3D;
};


float get_3D_Distance(float Self_x, float Self_y, float Self_z, float Object_x, float Object_y,
					  float Object_z)
{
	float x, y, z;
	x = Self_x - Object_x;
	y = Self_y - Object_y;
	z = Self_z - Object_z;
	// Calcular raiz cuadrada
	return (float)(sqrt(x * x + y * y + z * z));
}
float getDistance(Vector3A Pos1, Vector3A Pos2)
{
    return sqrt(pow(Pos2.X - Pos1.X, 2) + pow(Pos2.Y - Pos1.Y, 2) + pow(Pos2.Z - Pos1.Z, 2));
}
// Calcular coordenadas de rotacion
Vector2A rotateCoord(float angle, float objRadar_x, float objRadar_y)
{
	Vector2A radarCoordinate;
	float s = sin(angle * PI / 180);
	float c = cos(angle * PI / 180);
	radarCoordinate.X = objRadar_x * c + objRadar_y * s;
	radarCoordinate.Y = -objRadar_x * s + objRadar_y * c;
	return radarCoordinate;
}

Vector2A WorldToScreen(Vector3A obj, float matrix[16], float ViewW)
{
	float x =
		px + (matrix[0] * obj.X + matrix[4] * obj.Y + matrix[8] * obj.Z + matrix[12]) / ViewW * px;
	float y =
		py - (matrix[1] * obj.X + matrix[5] * obj.Y + matrix[9] * obj.Z + matrix[13]) / ViewW * py;

	return Vector2A(x, y);
}
Vector3A MarixToVector(FMatrix matrix)
{
	return Vector3A(matrix.M[3][0], matrix.M[3][1], matrix.M[3][2]);
}

FMatrix MatrixMulti(FMatrix m1, FMatrix m2)
{
	FMatrix matrix = FMatrix();
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			for (int k = 0; k < 4; k++)
			{
				matrix.M[i][j] += m1.M[i][k] * m2.M[k][j];
			}
		}
	}
	return matrix;
}

FMatrix TransformToMatrix(FTransform transform)
{
	FMatrix matrix;
	matrix.M[3][0] = transform.Translation.X;
	matrix.M[3][1] = transform.Translation.Y;
	matrix.M[3][2] = transform.Translation.Z;
	float x2 = transform.Rotation.X + transform.Rotation.X;
	float y2 = transform.Rotation.Y + transform.Rotation.Y;
	float z2 = transform.Rotation.Z + transform.Rotation.Z;
	float xx2 = transform.Rotation.X * x2;
	float yy2 = transform.Rotation.Y * y2;
	float zz2 = transform.Rotation.Z * z2;
	matrix.M[0][0] = (1 - (yy2 + zz2)) * transform.Scale3D.X;
	matrix.M[1][1] = (1 - (xx2 + zz2)) * transform.Scale3D.Y;
	matrix.M[2][2] = (1 - (xx2 + yy2)) * transform.Scale3D.Z;
	float yz2 = transform.Rotation.Y * z2;
	float wx2 = transform.Rotation.W * x2;
	matrix.M[2][1] = (yz2 - wx2) * transform.Scale3D.Z;
	matrix.M[1][2] = (yz2 + wx2) * transform.Scale3D.Y;
	float xy2 = transform.Rotation.X * y2;
	float wz2 = transform.Rotation.W * z2;
	matrix.M[1][0] = (xy2 - wz2) * transform.Scale3D.Y;
	matrix.M[0][1] = (xy2 + wz2) * transform.Scale3D.X;
	float xz2 = transform.Rotation.X * z2;
	float wy2 = transform.Rotation.W * y2;
	matrix.M[2][0] = (xz2 + wy2) * transform.Scale3D.Z;
	matrix.M[0][2] = (xz2 - wy2) * transform.Scale3D.X;
	matrix.M[0][3] = 0;
	matrix.M[1][3] = 0;
	matrix.M[2][3] = 0;
	matrix.M[3][3] = 1;
	return matrix;
}

FTransform getBone(unsigned long addr)
{
	FTransform transform;
	driver->read(addr, &transform, 4 * 11);
	return transform;
}



struct D3DXMATRIX
{
	float _11;
	float _12;
	float _13;
	float _14;
	float _21;
	float _22;
	float _23;
	float _24;
	float _31;
	float _32;
	float _33;
	float _34;
	float _41;
	float _42;
	float _43;
	float _44;
};

struct D3DXVECTOR4
{
	float X;
	float Y;
	float Z;
	float W;
};

struct FTransform1
{
	D3DXVECTOR4 Rotation;
	D3DVector Translation;
	D3DVector Scale3D;
};

D3DXMATRIX ToMatrixWithScale(D3DXVECTOR4 Rotation, D3DVector Translation, D3DVector Scale3D)
{
	D3DXMATRIX M;
	float X2, Y2, Z2, xX2, Yy2, Zz2, Zy2, Wx2, Xy2, Wz2, Zx2, Wy2;
	M._41 = Translation.X;
	M._42 = Translation.Y;
	M._43 = Translation.Z;
	X2 = Rotation.X + Rotation.X;
	Y2 = Rotation.Y + Rotation.Y;
	Z2 = Rotation.Z + Rotation.Z;
	xX2 = Rotation.X * X2;
	Yy2 = Rotation.Y * Y2;
	Zz2 = Rotation.Z * Z2;
	M._11 = (1 - (Yy2 + Zz2)) * Scale3D.X;
	M._22 = (1 - (xX2 + Zz2)) * Scale3D.Y;
	M._33 = (1 - (xX2 + Yy2)) * Scale3D.Z;
	Zy2 = Rotation.Y * Z2;
	Wx2 = Rotation.W * X2;
	M._32 = (Zy2 - Wx2) * Scale3D.Z;
	M._23 = (Zy2 + Wx2) * Scale3D.Y;
	Xy2 = Rotation.X * Y2;
	Wz2 = Rotation.W * Z2;
	M._21 = (Xy2 - Wz2) * Scale3D.Y;
	M._12 = (Xy2 + Wz2) * Scale3D.X;
	Zx2 = Rotation.X * Z2;
	Wy2 = Rotation.W * Y2;
	M._31 = (Zx2 + Wy2) * Scale3D.Z;
	M._13 = (Zx2 - Wy2) * Scale3D.X;
	M._14 = 0;
	M._24 = 0;
	M._34 = 0;
	M._44 = 1;
	return M;
}

FTransform1 ReadFTransform(long int address)
{
	FTransform1 Result;
	Result.Rotation.X = driver->read<float>(address);	// Rotation_X 
	Result.Rotation.Y = driver->read<float>(address + 4);	// Rotation_y
	Result.Rotation.Z = driver->read<float>(address + 8);	// Rotation_z
	Result.Rotation.W = driver->read<float>(address + 12);	// Rotation_w
	Result.Translation.X = driver->read<float>(address + 16);	// /Translation_X
	Result.Translation.Y = driver->read<float>(address + 20);	// Translation_y
	Result.Translation.Z = driver->read<float>(address + 24);	// Translation_z
	Result.Scale3D.X = driver->read<float>(address + 32);;	// Scale_X
	Result.Scale3D.Y = driver->read<float>(address + 36);;	// Scale_y
	Result.Scale3D.Z = driver->read<float>(address + 40);;	// Scale_z
	return Result;
}

// Obtener coordenadas 3D de huesos
D3DVector D3dMatrixMultiply(D3DXMATRIX bonematrix, D3DXMATRIX actormatrix)
{
	D3DVector result;
	result.X =
		bonematrix._41 * actormatrix._11 + bonematrix._42 * actormatrix._21 +
		bonematrix._43 * actormatrix._31 + bonematrix._44 * actormatrix._41;
	result.Y =
		bonematrix._41 * actormatrix._12 + bonematrix._42 * actormatrix._22 +
		bonematrix._43 * actormatrix._32 + bonematrix._44 * actormatrix._42;
	result.Z =
		bonematrix._41 * actormatrix._13 + bonematrix._42 * actormatrix._23 +
		bonematrix._43 * actormatrix._33 + bonematrix._44 * actormatrix._43;
	return result;
}

D3DVector getBoneXYZ(long int humanAddr, long int boneAddr, int Part)
{
	// Obtener datos Bone
	FTransform1 Bone = ReadFTransform(boneAddr + Part * 48);
	// Obtener datos Actor
	FTransform1 Actor = ReadFTransform(humanAddr);
	D3DXMATRIX Bone_Matrix = ToMatrixWithScale(Bone.Rotation, Bone.Translation, Bone.Scale3D);
	D3DXMATRIX Component_ToWorld_Matrix =
		ToMatrixWithScale(Actor.Rotation, Actor.Translation, Actor.Scale3D);
	D3DVector result = D3dMatrixMultiply(Bone_Matrix, Component_ToWorld_Matrix);
	return result;
}
Vector2A BoneToScreenPoint(Vector3A ObjCoord) {
	Vector2A RetPoint;

	float camera = MatrixArray[3] * ObjCoord.X + MatrixArray[7] * ObjCoord.Y + MatrixArray[11] * ObjCoord.Z + MatrixArray[15];
	RetPoint.X = px + (MatrixArray[0] * ObjCoord.X + MatrixArray[4] * ObjCoord.Y + MatrixArray[8] * ObjCoord.Z + MatrixArray[12]) / camera * px;
	RetPoint.Y = py - (MatrixArray[1] * ObjCoord.X + MatrixArray[5] * ObjCoord.Y + MatrixArray[9] * ObjCoord.Z + MatrixArray[13]) / camera * py;

	return RetPoint;
}
Vector3A GetBoneCoord(uint64_t BoneMatrix, uint64_t BoneObj, int BoneIndex) {

	FTransform meshtrans = getBone(BoneMatrix);
	FMatrix c2wMatrix = TransformToMatrix(meshtrans);

	FTransform headtrans = getBone(BoneObj + BoneIndex * 48);
	FMatrix boneMatrix = TransformToMatrix(headtrans);
	Vector3A relLocation = MarixToVector(MatrixMulti(boneMatrix, c2wMatrix));
	return relLocation;
}
void OffScreen(Vector2A Obj, float camear, ImColor color, float Radius)
{
	ImRect screen_rect = { 0.0f, 0.0f, (float)px * 2, (float)py * 2 };
	auto screen_center = screen_rect.GetCenter();
	auto angle = atan2(screen_center.y - Obj.Y, screen_center.x - Obj.X);
	angle += camear > 0 ? PI : 0.0f;
	Vector2A arrow_center{
			screen_center.x + Radius * cosf(angle),
			screen_center.y + Radius * sinf(angle)
	};
	std::array<ImVec2, 4>points{
			ImVec2(-22.0f, -8.6f),
			ImVec2(0.0f, 0.0f),
			ImVec2(-22.0f, 8.6f),
			ImVec2(-18.0f, 0.0f)
	};
	for (auto& point : points)
	{
		auto x = point.x * 1.155f;
		auto y = point.y * 1.155f;
		point.x = arrow_center.X + x * cosf(angle) - y * sinf(angle);
		point.y = arrow_center.Y + x * sinf(angle) + y * cosf(angle);
	}
	float alpha = 1.0f;
	if (camear > 0)
	{
		constexpr float nearThreshold = 200 * 200;
		ImVec2 screen_outer_diff = {
				Obj.X < 0 ? abs(Obj.X) : (Obj.X > screen_rect.Max.x ? Obj.X - screen_rect.Max.x : 0.0f),
				Obj.Y < 0 ? abs(Obj.Y) : (Obj.Y > screen_rect.Max.y ? Obj.Y - screen_rect.Max.y : 0.0f),
		};
		float distance = static_cast<float>(pow(screen_outer_diff.x, 2) + pow(screen_outer_diff.y, 2));
		alpha = camear < 0 ? 1.0f : (distance / nearThreshold);
	}
	ImColor arrowColor = color;
	arrowColor.Value.w = std::min(alpha, 1.0f);
	ImGui::GetBackgroundDrawList()->AddTriangleFilled(points[0], points[1], points[3], arrowColor);
	ImGui::GetBackgroundDrawList()->AddTriangleFilled(points[2], points[1], points[3], arrowColor);
	ImGui::GetBackgroundDrawList()->AddQuad(points[0], points[1], points[2], points[3], ImColor(0.0f, 0.0f, 0.0f, alpha), 1.335f);
}
double ArcToAngle(double angle)
{
    return angle * (double)57.29577951308;
}

void DrawEnemySight(float PlayerYaw, Vector3A BoneHeadXYZ, ImColor Color, float Thickness) {
	Vector3A SightEndXYZ;

	SightEndXYZ.X = BoneHeadXYZ.X + cos(PlayerYaw * PI / 180) * 100;
	SightEndXYZ.Y = BoneHeadXYZ.Y + sin(PlayerYaw * PI / 180) * 100;
	SightEndXYZ.Z = BoneHeadXYZ.Z + sin(0 * PI / 180) * 100;

	Vector2A SightStartScreenPoint = BoneToScreenPoint(BoneHeadXYZ);
	Vector2A SightEndScreenPoint = BoneToScreenPoint(SightEndXYZ);

	ImGui::GetForegroundDrawList()->AddLine({ SightStartScreenPoint.X, SightStartScreenPoint.Y }, { SightEndScreenPoint.X, SightEndScreenPoint.Y }, Color, Thickness);

}

Vector2A MemoryAimbotAlgo(Vector3A ZZpos, Vector3A  DRpos) {

	Vector2A PointingAngle;
	Vector3A zbc;

	zbc.X = DRpos.X - ZZpos.X;
	zbc.Y = DRpos.Y - ZZpos.Y;
	zbc.Z = DRpos.Z - ZZpos.Z;

	double pfg = std::sqrt((zbc.X * zbc.X) + (zbc.Y * zbc.Y));

	PointingAngle.X = std::atan2(zbc.Y, zbc.X) * 180 / PI;
	PointingAngle.Y = std::atan2(zbc.Z, pfg) * 180 / PI;

	return PointingAngle;
}


FMatrix RotToMatrix(FRotator rotation) {
	float radPitch = rotation.Pitch * ((float)M_PI / 180.0f);
	float radYaw = rotation.Yaw * ((float)M_PI / 180.0f);
	float radRoll = rotation.Roll * ((float)M_PI / 180.0f);

	float SP = sinf(radPitch);
	float CP = cosf(radPitch);
	float SY = sinf(radYaw);
	float CY = cosf(radYaw);
	float SR = sinf(radRoll);
	float CR = cosf(radRoll);

	FMatrix matrix;

	matrix.M[0][0] = (CP * CY);
	matrix.M[0][1] = (CP * SY);
	matrix.M[0][2] = (SP);
	matrix.M[0][3] = 0;

	matrix.M[1][0] = (SR * SP * CY - CR * SY);
	matrix.M[1][1] = (SR * SP * SY + CR * CY);
	matrix.M[1][2] = (-SR * CP);
	matrix.M[1][3] = 0;

	matrix.M[2][0] = (-(CR * SP * CY + SR * SY));
	matrix.M[2][1] = (CY * SR - CR * SP * SY);
	matrix.M[2][2] = (CR * CP);
	matrix.M[2][3] = 0;

	matrix.M[3][0] = 0;
	matrix.M[3][1] = 0;
	matrix.M[3][2] = 0;
	matrix.M[3][3] = 1;

	return matrix;
}

struct MinimalViewInfo {
	Vector3A Location;
	FRotator Rotation;
	float FOV;
};

Vector2A WorldToScreen(Vector3A worldLocation, MinimalViewInfo camViewInfo) {
	FMatrix tempMatrix = RotToMatrix(camViewInfo.Rotation);

	Vector3A vAxisX(tempMatrix.M[0][0], tempMatrix.M[0][1], tempMatrix.M[0][2]);
	Vector3A vAxisY(tempMatrix.M[1][0], tempMatrix.M[1][1], tempMatrix.M[1][2]);
	Vector3A vAxisZ(tempMatrix.M[2][0], tempMatrix.M[2][1], tempMatrix.M[2][2]);

	Vector3A vDelta = worldLocation - camViewInfo.Location;

	Vector3A vTransformed(Vector3A::dot(vDelta, vAxisY), Vector3A::dot(vDelta, vAxisZ), Vector3A::dot(vDelta, vAxisX));

	// Si el objeto esta detras de la camara, no proyectar en pantalla
	if (vTransformed.Z < 1.0f) {
		return Vector2A(-9999.0f, -9999.0f);
	}

	float fov = camViewInfo.FOV;
	float screenCenterX = px;
	float screenCenterY = py;

	return Vector2A(
		(screenCenterX + vTransformed.X * (screenCenterX / tanf(fov * ((float)M_PI / 360.0f))) / vTransformed.Z),
		(screenCenterY - vTransformed.Y * (screenCenterX / tanf(fov * ((float)M_PI / 360.0f))) / vTransformed.Z)
	);
}

std::map<long, Vector3A> PosMap;
inline void DrawTextStroke(float size, int x, int y, ImVec4 color, const char* str)
{
	ImGui::GetForegroundDrawList()->AddText(NULL, size, ImVec2(x + 1, y), ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 1.0f)), str);
	ImGui::GetForegroundDrawList()->AddText(NULL, size, ImVec2(x - 0.1, y), ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 1.0f)), str);
	ImGui::GetForegroundDrawList()->AddText(NULL, size, ImVec2(x, y + 1), ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 1.0f)), str);
	ImGui::GetForegroundDrawList()->AddText(NULL, size, ImVec2(x, y - 1), ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 1.0f)), str);
	ImGui::GetForegroundDrawList()->AddText(NULL, size, ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(color), str);
}
inline void DrawTextBold(float size, float x, float y, ImColor color, ImColor color1, const char* str)
{
    ImGui::GetBackgroundDrawList()->AddText(NULL, size, ImVec2(x-0.1, y-0.1), color1, str);
    ImGui::GetBackgroundDrawList()->AddText(NULL, size, ImVec2(x+0.1, y+0.1), color1, str);
    ImGui::GetBackgroundDrawList()->AddText(NULL, size, ImVec2(x, y), color, str);
}

inline Vector2A DrawRadar(Vector2A MYxy, Vector2A Dxy, float MyYaw, float X, float Y) {
	Dxy.X = (Dxy.X - MYxy.X) / 120;
	Dxy.Y = (Dxy.Y - MYxy.Y) / 120;

	float R = std::sqrt(Dxy.X * Dxy.X + Dxy.Y * Dxy.Y);

	float Angle;
	if (Dxy.X > 0) {
		Angle = std::atan(Dxy.Y / Dxy.X) * 180 / PI;
	}
	else if (Dxy.X < 0 && Dxy.Y >= 0) {
		Angle = (std::atan(Dxy.Y / Dxy.X) + PI) * 180 / PI;
	}
	else if (Dxy.X < 0 && Dxy.Y < 0) {
		Angle = (std::atan(Dxy.Y / Dxy.X) - PI) * 180 / PI;
	}
	else {
		if (Dxy.Y > 0) {
			Angle = 90;
		}
		else if (Dxy.Y < 0) {
			Angle = -90;
		}
		else {
			Angle = 0;
		}
	}

	Angle = MyYaw - Angle;
	Angle = Angle * PI / 180;

	Vector2A Point;
	Point.X = X - (R * std::sin(Angle));
	Point.Y = Y - (R * std::cos(Angle));

	Point.X = std::max(0.0f, std::min((float)px * 2, Point.X));
	Point.Y = std::max(0.0f, std::min((float)py * 2, Point.Y));

	return Point;
}
	std::map<Vector2A, int> drawn_positions;  // Usado para registrar el numero de veces que se dibuja cada posicion
Vector3A GetTrueLocation(long human, long Bone) {
    Vector3A result;
    FTransform Mheadtrans = driver->read<FTransform>(Bone + 82 * 48);
    FTransform Mmeshtrans = driver->read<FTransform>(human);
    FMatrix Mc2wMatrix = TransformToMatrix(Mmeshtrans);
    FMatrix MboneMatrix = TransformToMatrix(Mheadtrans);
    Vector3A RR = MarixToVector(MatrixMulti(MboneMatrix, Mc2wMatrix));
    RR.Z += 7; // Longitud del cuello
    return result = RR;
}
void DrawBox(float X,float top,float W,float bottom)
{
ImGui::GetBackgroundDrawList()->AddLine({X, top}, {X+(W/4), top}, White, 2.5f);
ImGui::GetBackgroundDrawList()->AddLine({X+W, top}, {X+W-(W/4), top},White, 2.5f);
ImGui::GetBackgroundDrawList()->AddLine({X, top}, {X, top+(W/4)},White, 2.5f);
ImGui::GetBackgroundDrawList()->AddLine({X+W, top}, {X+W, top+(W/4)},White, 2.5f);
ImGui::GetBackgroundDrawList()->AddLine({X, bottom}, {X+(W/4), bottom},White, 2.5f);
ImGui::GetBackgroundDrawList()->AddLine({X+W, bottom}, {X+W-(W/4), bottom},White, 2.5f);
ImGui::GetBackgroundDrawList()->AddLine({X, bottom}, {X, bottom-(W/4)},White, 2.5f);
ImGui::GetBackgroundDrawList()->AddLine({X+W, bottom}, {X+W, bottom-(W/4)},White, 2.5f);      
}
Vector2A Head; Vector2A Chest; Vector2A Pelvis; Vector2A Left_Shoulder; Vector2A Right_Shoulder; Vector2A Left_Elbow; Vector2A Right_Elbow;  Vector2A Left_Wrist; Vector2A Right_Wrist; Vector2A Left_Thigh; Vector2A Right_Thigh;Vector2A Left_Knee;Vector2A Right_Knee;Vector2A Left_Ankle;Vector2A Right_Ankle;