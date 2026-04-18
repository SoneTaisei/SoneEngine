#pragma once

struct Vector3 {
	float x;
	float y;
	float z;

	// スカラー倍
	Vector3 operator*(float scalar) const {
		return { x * scalar, y * scalar, z * scalar };
	}

	// スカラー除算（必要なら）
	Vector3 operator/(float scalar) const {
		return { x / scalar, y / scalar, z / scalar };
	}

	// 加算
	Vector3 operator+(const Vector3 &other) const {
		return { x + other.x, y + other.y, z + other.z };
	}

	// 減算
	Vector3 operator-(const Vector3 &other) const {
		return { x - other.x, y - other.y, z - other.z };
	}

	// 代入付き加算
	Vector3 &operator+=(const Vector3 &other) {
		x += other.x; y += other.y; z += other.z;
		return *this;
	}

	// 代入付き減算
	Vector3 &operator-=(const Vector3 &other) {
		x -= other.x; y -= other.y; z -= other.z;
		return *this;
	}
};
