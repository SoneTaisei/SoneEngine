#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"
#include "Matrix3x3.h"

inline Vector3 operator*(const Matrix4x4& mat, const Vector3& vec) {
	Vector3 result;
	// 方向ベクトルのため、w成分は0として計算
	result.x = vec.x * mat.m[0][0] + vec.y * mat.m[1][0] + vec.z * mat.m[2][0];
	result.y = vec.x * mat.m[0][1] + vec.y * mat.m[1][1] + vec.z * mat.m[2][1];
	result.z = vec.x * mat.m[0][2] + vec.y * mat.m[1][2] + vec.z * mat.m[2][2];
	// w成分は方向ベクトルのため無視
	return result;
}

inline Matrix4x4 operator*(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result{};

	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			result.m[row][col] =
				m1.m[row][0] * m2.m[0][col] +
				m1.m[row][1] * m2.m[1][col] +
				m1.m[row][2] * m2.m[2][col] +
				m1.m[row][3] * m2.m[3][col];
		}
	}

	return result;
}

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
