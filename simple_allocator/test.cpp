#include "lyalloc.h"
#include <vector>
using namespace std;
int main()
{
    int buf[5] = {0,1,2,3,4};
    int i;
    vector<int, JJ::allocator<int> > vec(buf, buf+5);
    for (i=0; i<vec.size(); i++)
        cout << vec[i] << endl;

    return 0;
}
