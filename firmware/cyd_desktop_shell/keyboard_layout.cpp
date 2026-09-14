#include "screen/keyboard.h"

namespace cyd::desktop::screen::keyboard {
namespace {

const char *kKeyLabelsLower[kKeyCount] = {
    "1","2","3","4","5","6","7","8","9","0","BS",
    "q","w","e","r","t","y","u","i","o","p","[",
    "CAP","a","s","d","f","g","h","j","k","l","]",
    "SYM","z","x","c","v","b","n","m",",",".","=",
    "ESC","SPACE",":","\"","(",")","ENTER"
};
const char *kKeyLabelsUpper[kKeyCount] = {
    "1","2","3","4","5","6","7","8","9","0","BS",
    "Q","W","E","R","T","Y","U","I","O","P","{",
    "CAP","A","S","D","F","G","H","J","K","L","}",
    "SYM","Z","X","C","V","B","N","M","<",">","+",
    "ESC","SPACE",";","'","(",")","ENTER"
};
const char *kKeyLabelsSym[kKeyCount] = {
    "!","@","#","$","%","^","&","*","(",")","BS",
    "<",">","~","/","\\","|","?",";",":","%","[",
    "CAP","{","}","+","-","*","/","=","_","'","`",
    "SYM","!","@","#","$","&","|","^","\"","/","=",
    "ESC","SPACE","_","-","*","/","ENTER"
};

const char kKeyCharLower[kKeyCount] = {
    '1','2','3','4','5','6','7','8','9','0','\x08',
    'q','w','e','r','t','y','u','i','o','p','[',
    '\x01','a','s','d','f','g','h','j','k','l',']',
    '\x02','z','x','c','v','b','n','m',',','.','=',
    '\x1b',' ',':','"','(',')','\n'
};
const char kKeyCharUpper[kKeyCount] = {
    '1','2','3','4','5','6','7','8','9','0','\x08',
    'Q','W','E','R','T','Y','U','I','O','P','{',
    '\x01','A','S','D','F','G','H','J','K','L','}',
    '\x02','Z','X','C','V','B','N','M','<','>','+',
    '\x1b',' ',';','\'','(',')','\n'
};
const char kKeyCharSym[kKeyCount] = {
    '!','@','#','$','%','^','&','*','(',')','\x08',
    '<','>','~','/','\\','|','?',';',':','%','[',
    '\x01','{','}','+','-','*','/','=','_','\'','`',
    '\x02','!','@','#','$','&','|','^','"','/','=',
    '\x1b',' ','_','-','*','/','\n'
};

}  // namespace

KeyRect key_rect(int index) {
    int kx = 0, ky = 0, kw = 0, kh = 17;
    if (index < 44) {
        const int r = index / 11;
        const int c = index % 11;
        ky = 117 + r * 19;
        kx = 2 + c * 29;
        kw = (c == 10) ? 26 : 27;
    } else {
        ky = 193;
        kh = 18;
        switch (index) {
            case 44: kx = 2; kw = 27; break;
            case 45: kx = 31; kw = 100; break;
            case 46: kx = 133; kw = 24; break;
            case 47: kx = 159; kw = 24; break;
            case 48: kx = 185; kw = 24; break;
            case 49: kx = 211; kw = 24; break;
            default: kx = 237; kw = 81; break;
        }
    }
    return {kx, ky, kw, kh};
}

int key_at(int x, int y) {
    if (x < 0 || x >= 320 || y < 117 || y >= 212) return -1;
    if (y < 193) {
        int row = (y - 117) / 19;
        if (row > 3) row = 3;
        int col = (x - 2) / 29;
        if (col < 0) col = 0;
        if (col > 10) col = 10;
        return row * 11 + col;
    }
    if (x < 31) return 44;
    if (x < 133) return 45;
    if (x < 159) return 46;
    if (x < 185) return 47;
    if (x < 211) return 48;
    if (x < 237) return 49;
    return 50;
}

const char *key_label(uint8_t mode, int index) {
    const char **labels = (mode == kUpper) ? kKeyLabelsUpper
        : ((mode == kSymbols) ? kKeyLabelsSym : kKeyLabelsLower);
    return labels[index];
}

char key_char(uint8_t mode, int index) {
    const char *table = (mode == kUpper) ? kKeyCharUpper
        : ((mode == kSymbols) ? kKeyCharSym : kKeyCharLower);
    return table[index];
}

}  // namespace cyd::desktop::screen::keyboard
