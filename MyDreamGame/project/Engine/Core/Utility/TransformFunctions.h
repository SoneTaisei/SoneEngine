#pragma once
#include "Vector3.h"

struct Matrix4x4 {
	float m[4][4];
};

struct Matrix3x3 {
	float m[3][3];
};

class TransformFunctions {
public:
	static Matrix4x4 MakeRoteXMatrix(float radian);
	static Matrix4x4 MakeRoteYMatrix(float radian);
	static Matrix4x4 MakeRoteZMatrix(float radian);
	static Matrix4x4 MakeTranslateMatrix(const Vector3 &translate);
	static Matrix4x4 MakeScaleMatrix(const Vector3 &scale);
	static Vector3 Transform(const Vector3 &vector, const Matrix4x4 &matrix);
	static Matrix4x4 MakeAffineMatrix(const Vector3 &scale, const Vector3 &rotate, const Vector3 &translate);
	static Matrix4x4 Add(const Matrix4x4 &matrix1, const Matrix4x4 &matrix2);
	static Vector3 AddV(const Vector3 a, const Vector3 b);
	static Matrix4x4 Subtract(const Matrix4x4 &matrix1, const Matrix4x4 &matrix2);
	static Vector3 SubtractV(const Vector3 &a, const Vector3 &b);
	static Matrix4x4 Multiply(const Matrix4x4 &matrix1, const Matrix4x4 &matrix2);
	static Vector3 MultiplyV(float scalar, Vector3 vector);
	static Matrix4x4 Inverse(const Matrix4x4 &m);
	static Matrix4x4 Transpose(const Matrix4x4 &m);
	static Matrix4x4 MakeIdentity4x4();
	static Vector3 Cross(const Vector3 &v1, const Vector3 &v2);
	// 1.透視投影行列
	static Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);
	// 2.正射影行列
	static Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);
	// 3.ビューポート変換行列
	static Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

	static Vector3 Normalize(Vector3 v);
    static Matrix4x4 MakeViewMatrix(const Vector3 &rotate, const Vector3 &translate);


};
