// Computing A*B (mod N) Efficiently in ANSI C, Henry G. Baker
extern "C" long long fast_modmul(long long a, long long b, long long M) {
	long long res = a * b - M * (long long)(1.L / M * a * b);
  if (res < 0) res += M;
  if (res >= M) res -= M;
	return res;
}