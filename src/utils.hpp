#pragma once
#include <cmath>
#include <concepts>

template <typename T>
	requires std::integral<T> || std::floating_point<T>
struct Vector3 {
	T x, y, z;

	T sqrMagnitude() const { return x * x + y * y + z * z; }
	T magnitude() const { return std::sqrtf(sqrMagnitude()); }
	Vector3 normalized() const { return *this / magnitude(); }

	Vector3& operator+=(const Vector3& other) {
		x += other.x;
		y += other.y;
		z += other.z;
		return *this;
	}
	Vector3& operator-=(const Vector3& other) {
		x -= other.x;
		y -= other.y;
		z -= other.z;
		return *this;
	}
	Vector3& operator*=(const T& scalar) {
		x *= scalar;
		y *= scalar;
		z *= scalar;
		return *this;
	}
	Vector3& operator/=(const T& scalar) {
		x /= scalar;
		y /= scalar;
		z /= scalar;
		return *this;
	}
	Vector3 operator+(const Vector3& other) const { return { x + other.x, y + other.y, z + other.z }; }
	Vector3 operator-(const Vector3& other) const { return { x - other.x, y - other.y, z - other.z }; }

	Vector3 operator%(const T& scalar) const { return { x % scalar, y % scalar, z % scalar }; }
	bool operator==(const Vector3& other) const {
		return (this->x == other.x) && (this->y == other.y) && (this->z == other.z);
	}

	Vector3 operator*(const T& scalar) const {
		Vector3 result(*this);
		result *= scalar;
		return result;
	}
	Vector3 operator/(const T& scalar) const {
		Vector3 result(*this);
		result /= scalar;
		return result;
	}

	Vector3() : x(0), y(0), z(0) {}
	Vector3(T x, T y, T z) : x(x), y(y), z(z) {}
};

template <typename T>
struct std::hash<Vector3<T>> {
	std::size_t operator()(const Vector3<T>& v) const noexcept {
		std::size_t h1 = std::hash<T>{}(v.x);
		std::size_t h2 = std::hash<T>{}(v.y);
		std::size_t h3 = std::hash<T>{}(v.z);
		return h1 ^ (h2 << 1) ^ (h3 << 2);
	}
};

using Vector3f = Vector3<float>;