#include <dualforge/dualforge.hpp>
int main() {
    dualforge::Context context;
    if (!context.status()) return 2;
    return dualforge::effects::off()[0]==5?0:1;
}
