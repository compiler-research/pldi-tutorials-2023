#include <iostream>
#include <vector>
#include <random>
using namespace std;

constexpr int modmul(long long a, long long b, int M) {
  return a*b%M;
}

int ilog2(int n) {
  int res = 0;
  while (n >>= 1) res++;
  return res;
}

int binexp(long long a, long long n, int MOD) {
  long long res = 1;
  while (n) {
    if (n&1) res = modmul(res, a, MOD);
    a = modmul(a, a, MOD);
    n /= 2;
  }
  return res;
}

void fft(long long* a, int m, long long omega, int MOD) {
  if (m == 1) return;
  fft(a, m/2,modmul(omega,omega,MOD), MOD);
  fft(a+m/2,m/2,modmul(omega,omega,MOD), MOD);
  long long cur = 1;
  for (int i=0;i<m/2;i++,cur=modmul(cur,omega,MOD)){
    long long even = a[i], odd = a[i+m/2];
    a[i] = (even + modmul(cur, odd, MOD)) % MOD;
    a[i+m/2] = (even - modmul(cur,odd, MOD) + MOD) % MOD;
  }
}

void fft(vector<long long>& a, int MOD, bool inv) {
  const int m = a.size();
  for (int i=1,j=0;i<m;i++){
    int bit = m >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j) swap(a[i], a[j]);
  }
  long long omega = binexp(3, (MOD-1)/m, MOD);
  fft(a.data(),m,omega,MOD);
  if (inv) {
    long long minv = binexp(m, MOD-2, MOD);
    reverse(a.begin()+1,a.end());
    for (long long& v : a) v = modmul(v, minv, MOD);
  }
}

vector<long long> convolution(const vector<long long>& a, const vector<long long>& b, int MOD) {
  const int n = a.size()+b.size()-1;
  const int m = 2 << ilog2(n);
  vector<long long> a2(m), b2(m);
  copy(a.begin(), a.end(), a2.begin());
  copy(b.begin(), b.end(), b2.begin());
  fft(a2, MOD, false), fft(b2, MOD, false);
  for (int i=0;i<m;i++) a2[i] = modmul(a2[i],b2[i],MOD);
  fft(a2, MOD, true);
  a2.resize(n);
  return a2;
}

int main() {
  const int MOD = 998244353;
  int n = 3e6;
  vector<long long> a(n), b(n);
  mt19937 gen(5353); 
  uniform_int_distribution<> distr(0, 50);
  for (int i=0;i<n;i++) a[i] = distr(gen);
  for (int i=0;i<n;i++) b[i] = distr(gen);
  vector<long long> res = convolution(a,b,MOD);
  long long sum = 0;
  for (int a : res) sum += a;
  cout << sum << "\n";
}
