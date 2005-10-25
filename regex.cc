#include <string>
#include <bitset>
#include <cctype>
#include <algorithm>
#include <iostream>
#include <vector>
#include <list>

/* Extended regular expression optimizer */
/* Copyright (C) 1992,2004 Bisqwit (http://iki.fi/bisqwit/) */

using namespace std;

static const unsigned uinf = ~0U;

typedef bitset<256> charset;

typedef vector<struct item> sequence;
typedef list<sequence> choices;

struct item
{
    choices* tree;
    charset ch; // used if tree is 0.
    
    unsigned min;
    unsigned max;
    bool greedy;
    // bool bol;
    // bool eol;
    
    item(): tree(0),ch(),min(1),max(1),greedy(true)
    {
    }
    
    ~item()
    {
        //if(tree) delete tree;
    }
    
    bool is_equal(const item& b) const
    {
        if(greedy != b.greedy) return false;
        if(tree) return b.tree && *tree == *b.tree;
        if(b.tree) return false;
        return ch == b.ch;
    }
    
    bool is_combinable(const item& b) const
    {  
        if(!is_equal(b)) return false;
        
        if(min==b.min && max==b.max) return true;
        
        return false;
    }
    
    bool operator!=(const item& b) const { return !operator==(b); }
    bool operator==(const item& b) const
    {
        if(!tree != !b.tree
        || min != b.min
        || max != b.max
        || greedy != b.greedy) return false;
        if(tree) { return *tree == *b.tree; }
        return ch == b.ch;
    }
    
    void Optimize();
//private:
//    void operator= (const item&);
//    item(const item&);
};

void DumpTree(const choices& c, bool need_parens=false);
void DumpSequence(const sequence& s);
void DumpKey(const charset& s);

void OptimizeSequence(sequence& seq);
void OptimizeTree(choices& tree);

static void SumCounts(unsigned& target, unsigned value)
{
    if(target == uinf) return;
    if(value == uinf)
        target = value;
    else
        target += value;
}

static unsigned EqualBeginSequence(const sequence& a, const sequence& b)
{
    for(unsigned as = a.size(), bs = b.size(), c=0; ; ++c)
    {
        if(c >= as || c >= bs || a[c] != b[c]) return c;
    }
}
static unsigned EqualEndSequence(const sequence& a, const sequence& b)
{
    for(unsigned as = a.size(), bs = b.size(), c=0; ; ++c)
    {
        if(c >= as || c >= bs || a[as-c-1] != b[bs-c-1]) return c;
    }
}
static bool IsEqualEndSequence(const sequence& a, const sequence& b)
{
    return !a.empty() && !b.empty() && *(a.end()-1) == *(b.end()-1);
}
static bool IsEqualBeginSequence(const sequence& a, const sequence& b)
{
    return !a.empty() && !b.empty() && *a.begin() == *b.begin();
}
static const sequence CopyEndSequence(const sequence& a, unsigned n)
{
    sequence result;
    if(n > a.size()) n = a.size();
    result.insert(result.end(), a.end()-n, a.end());
    return result;
}
static const sequence CopyBeginSequence(const sequence& a, unsigned n)
{
    sequence result;
    if(n > a.size()) n = a.size();
    result.insert(result.end(), a.begin(), a.begin()+n);
    return result;
}
static void ClipBeginSequence(sequence& a, unsigned n)
{
    if(n > a.size()) n = a.size();
    a.erase(a.begin(), a.begin()+n);
}
static void ClipEndSequence(sequence& a, unsigned n)
{
    if(n > a.size()) n = a.size();
    a.erase(a.end()-n, a.end());
}

static void CompressSequence(sequence& seq)
{
    if(seq.empty()) return;
    
    // Convert ab{0}c to ac
    // Convert aaaa to a{4}
    
    unsigned prev=0;
    sequence result;
    result.reserve(seq.size());
    bool erased=false;
    for(unsigned a=1; a<seq.size(); ++a)
    {
        if(seq[a].max==0)
            erased=true;
        else if(seq[a].is_equal(seq[prev]))
        {
            SumCounts(seq[prev].min, seq[a].min);
            SumCounts(seq[prev].max, seq[a].max);
            erased=true;
        }
        else
        {
            result.push_back(seq[prev]);
            prev=a;
        }
    }
    if(erased || !seq[0].max)
    {
        if(seq[prev].max != 0) result.push_back(seq[prev]);
        seq = result;
    }
}
static void HeavyCompressSequence(sequence& seq)
{
    // Convert "abababcd" to "(ab){3}cd"
restart:
    for(unsigned a=0; a<seq.size(); ++a)
    {
        unsigned left = seq.size() - a;
        unsigned maxlen = left / 2;
        
        unsigned bestscore = 0;
        unsigned bestscore_len   = 0;
        unsigned bestscore_count = 0;
        
        for(unsigned m=1; m<=maxlen; ++m)
        {
            unsigned maxcount = left / m;
            unsigned score = m;
            for(unsigned c=1; c<=maxcount; ++c)
            {
                bool eq = equal(seq.begin()+a,
                                seq.begin()+a+m,
                                seq.begin()+a + m*c);
                if(!eq) break;

                score += m;
                if(score > bestscore)
                {
                    bestscore = score;
                    bestscore_len   = m;
                    bestscore_count = c+1;
                }
            }
        }
        if(bestscore)
        {
            sequence subseq;
            subseq.insert(subseq.end(), seq.begin()+a, seq.begin()+a+bestscore_len);
            
            choices* tmp = new choices;
            tmp->push_back(subseq);
            item it;
            it.tree = tmp;
            it.min = it.max = bestscore_count;
            it.Optimize();
            seq[a] = it;
            seq.erase(seq.begin()+a+1, seq.begin()+a+bestscore_len*bestscore_count);
            goto restart;
        }
    }
}
static void FlattenSequence(sequence& seq)
{
    // Convert a(bc) to abc
    
    sequence result;
    result.reserve(seq.size());
    
    for(unsigned a=0; a<seq.size(); ++a)
    {
        if(seq[a].tree
        && seq[a].min == 1
        && seq[a].max == 1
        && seq[a].tree->size() == 1)
        {
            sequence& seq2 = *seq[a].tree->begin();
            
            OptimizeSequence(seq2);
            
            result.insert(result.end(), seq2.begin(), seq2.end());
        }
        else
            result.push_back(seq[a]);
    }
    seq = result;
}

void OptimizeSequence(sequence& seq)
{
    for(sequence::iterator i=seq.begin(); i!=seq.end(); ++i)
        i->Optimize();
    
    FlattenSequence(seq);
    CompressSequence(seq);
}

static void FlattenTree(choices& tree)
{
    // Convert ((x|y)|z) to (x|y|z)  or ((abc)) to (abc)
    
    for(choices::iterator j,i=tree.begin(); i!=tree.end(); i=j)
    {
        j=i; ++j;

        const sequence& seq = *i;
        if(seq.size() != 1) continue;
        
        const item& it = seq[0];
        if(!it.tree || it.min != 1 || it.max != 1) continue;
        
        tree.insert(j, it.tree->begin(), it.tree->end());
        tree.erase(i);
    }
}

static void CharsetCombineTree(choices& tree)
{
    // Convert (a|d) to ([ad])

    charset ch;
    
    bool found_sets = false;
    for(choices::iterator j,i=tree.begin(); i!=tree.end(); i=j)
    {
        j=i; ++j;
        
        const sequence& seq = *i;
        if(seq.size() != 1) continue;
        
        const item& it = seq[0];
        if(it.tree || it.min != 1 || it.max != 1) continue;
        
        ch |= it.ch;
        found_sets = true;
        tree.erase(i);
    }
    if(found_sets)
    {
        item tmp;
        tmp.ch  = ch;
        sequence seq; seq.push_back(tmp);
        tree.push_back(seq);
    }

/*
    for(choices::iterator i=tree.begin(); i!=tree.end(); ++i)
    {
        const sequence& seq = *i;
        if(seq.size() != 1) { only_sets = false; break; }
        
        const item& it = seq[0];
        if(it.tree) continue;
        
        if(!it.greedy) continue;
        
        // we need a rangeset with possibility of open ends
    }
*/
}

static bool CombineTree(choices& tree)
{
    // (abc | dbc)   ->   ((a|d)bc)  -> [ad]bc
    
    // (abc | dbc | koo) -> ((a|d)bc | koo)
    
    // (abde | abcd | akoo) -> a(bde|bcd|koo) -> a(b(de|cd)|koo)
    
    // axc | ayc | azc -> a(x|y|z)c
    
    // (aXe | bXf | cXg)  NOT   [abc]X[efg] because cXe is not allowed.
    
    // (abc | abd | abe)
    //  -> (a(bc|bd)) | abe
    //  -> a(bc|bd|be) -> (a(b(c|d|e))) -> (a(b([cde]))) -> ab[c-e]
    
    // (aab | ab) -> a(ab|b) -> a(a|)b
    // a{1,2}b
    
    for(choices::iterator i=tree.begin(); i!=tree.end(); ++i)
    {
        list<choices::iterator> com;
        com.push_back(i);
        for(choices::iterator j=i; ++j!=tree.end(); )
        {
            if(IsEqualEndSequence(*i, *j)) com.push_back(j);
        }
        if(com.size() > 1)
        {
            sequence rep = CopyEndSequence(*i, 1);
            choices* subchoice = new choices;
            for(list<choices::iterator>::iterator
                k = com.begin(); k != com.end(); ++k)
            {
                ClipEndSequence(**k, 1);
                subchoice->push_back(**k);
                tree.erase(*k);
            }
            item it;
            it.tree = subchoice;
            it.Optimize();
            rep.insert(rep.begin(), it);
            tree.push_back(rep);
            return true;
        }
    }
    for(choices::iterator i=tree.begin(); i!=tree.end(); ++i)
    {
        list<choices::iterator> com;
        com.push_back(i);
        for(choices::iterator j=i; ++j!=tree.end(); )
        {
            if(IsEqualBeginSequence(*i, *j)) com.push_back(j);
        }
        if(com.size() > 1)
        {
            sequence rep = CopyBeginSequence(*i, 1);
            choices* subchoice = new choices;
            for(list<choices::iterator>::iterator
                k = com.begin(); k != com.end(); ++k)
            {
                ClipBeginSequence(**k, 1);
                subchoice->push_back(**k);
                tree.erase(*k);
            }
            item it;
            it.tree = subchoice;
            it.Optimize();
            rep.insert(rep.end(), it);
            tree.push_back(rep);
            return true;
        }
    }
    return false;
}

void OptimizeTree(choices& tree)
{   
    for(;;)
    {
        for(choices::iterator i=tree.begin(); i!=tree.end(); ++i)
            OptimizeSequence(*i);
        
        FlattenTree(tree);
        CharsetCombineTree(tree);

        /*
        cout << "CombineTree:\n";
        DumpTree(tree);
        cout << endl;
        */

        bool changed = CombineTree(tree);
        if(!changed) break;
        
        /*
        cout << "Changed:\n";
        DumpTree(tree);
        cout << endl;
        */
        
        FlattenTree(tree);
    }

    for(choices::iterator i=tree.begin(); i!=tree.end(); ++i)
        HeavyCompressSequence(*i);
}

void item::Optimize()
{
    if(tree)
    {
        OptimizeTree(*tree);
        
        // Convert (x) to x
        // Convert (x{5,7}) to x{5,7}
        // Convert (x){5,7} to x{5,7}
        // Not convert (x{2}){3}
        
        if(tree->size() == 1)
        {
            const sequence& seq = *tree->begin();
            if(seq.size() == 1)
            {
                const item& it = seq[0];
                
                if(min==1 && max==1)
                {
                    choices* tmp = tree;
                    *this = it;
                    delete tmp;
                }
                else if(it.min==1 && it.max==1)
                {
                    choices* tmp = tree;
                    tree = it.tree;
                    ch   = it.ch;
                    delete tmp;
                }
            }
        }
    }
}


static const charset& GetDotMask()
{
    static const struct data { charset result; data() {
    result.set('\n');
    result.flip();
    } } data;
    return data.result;
}
static const charset& GetDecMask()
{
    static const struct data { charset result; data() {
    for(unsigned a='0'; a<='9'; ++a) result[a]=true;
    } } data;
    return data.result;
}
static const charset& GetPSpaceMask()
{
    static const struct data { charset result; data() {
    result.set(' ');
    result.set('\t');
    result.set('\r');
    result.set('\n');
    result.set('\f');
    } } data;
    return data.result;
}
static const charset& GetSpaceMask()
{
    static const struct data { charset result; data() {
    result = GetPSpaceMask();
    result.set('\v');
    } } data;
    return data.result;
}
static const charset& GetUpperMask()
{
    static const struct data { charset result; data() {
    for(unsigned a='A'; a<='Z'; ++a) result.set(a);
    } } data;
    return data.result;
}
static const charset& GetLowerMask()
{
    static const struct data { charset result; data() {
    for(unsigned a='a'; a<='z'; ++a) result.set(a);
    } } data;
    return data.result;
}
static const charset& GetAlphaMask()
{
    static const struct data { charset result; data() {
    result = GetUpperMask() | GetLowerMask();
    } } data;
    return data.result;
}
static const charset& GetXdigitMask()
{
    static const struct data { charset result; data() {
    result = GetDecMask();
    for(unsigned a='A'; a<='F'; ++a) result.set(a);
    for(unsigned a='a'; a<='f'; ++a) result.set(a);
    } } data;
    return data.result;
}
static const charset& GetAlnumMask()
{
    static const struct data { charset result; data() {
    result = GetDecMask() | GetAlphaMask();
    } } data;
    return data.result;
}
static const charset& GetWordMask()
{
    static const struct data { charset result; data() {
    result = GetAlnumMask();
    result.set('_');
    } } data;
    return data.result;
}
static const charset& GetGraphMask()
{
    static const struct data { charset result; data() {
    for(unsigned a=0x21; a<=0x7E; ++a) result.set(a);
    } } data;
    return data.result;
}
static const charset& GetPrintMask()
{
    static const struct data { charset result; data() {
    result = GetGraphMask();
    result.set(' ');
    } } data;
    return data.result;
}
static const charset& GetPunctMask()
{
    static const struct data { charset result; data() {
    result = GetGraphMask() &~ GetAlnumMask();
    } } data;
    return data.result;
}
static const charset& GetAsciiMask()
{
    static const struct data { charset result; data() {
    for(unsigned a=0x00; a<=0x7F; ++a) result.set(a);
    } } data;
    return data.result;
}
static const charset& GetCntrlMask()
{
    static const struct data { charset result; data() {
    result = GetAsciiMask() &~ GetPrintMask();
    } } data;
    return data.result;
}

static const charset ParseEscape(const string& s, unsigned& pos)
{
    unsigned b=s.size();
    if(pos+1 < b) ++pos;
    char c = s[pos];
    switch(c)
    {
        case 't': { charset result; result.set('\t'); return result; }
        case 'n': { charset result; result.set('\n'); return result; }
        case 'r': { charset result; result.set('\r'); return result; }
        case 'v': { charset result; result.set('\v'); return result; }
        case 'f': { charset result; result.set('\f'); return result; }
        case 'a': { charset result; result.set('\a'); return result; }
        case 'e': { charset result; result.set(27); return result; }
        case 'c': { charset result; result.set(toupper(s[++pos]) ^ 64); return result; }
        case 'd': return GetDecMask();
        case 'D': return ~GetDecMask();
        case 's': return GetPSpaceMask();
        case 'S': return ~GetPSpaceMask();
        case 'w': return GetWordMask();
        case 'W': return ~GetWordMask();
        case 'x':
        {
            unsigned hex = 0, ndigits=0;
            for(; ndigits < 2 && pos+1 < b && isxdigit(s[pos+1]); ++ndigits)
            {
                c = s[++pos];
                hex = hex*16;
                if(isdigit(c)) hex += (c-'0');
                else if(islower(c)) hex += (c-'a'+10);
                else if(isupper(c)) hex += (c-'A'+10);
            }
            { charset result; result.set(hex); return result; }
        }
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
        {
            unsigned octal = 0, ndigits=0;
            for(--pos; ndigits < 3
              && pos+1 < b
              && s[pos+1] >= '0' && s[pos+1] <= '7'; ++ndigits)
            {
                c = s[++pos];
                octal = octal*8 + (c-'0');
            }
            { charset result; result.set(octal); return result; }
        }
        default: { charset result; result.set((unsigned char)c); return result; }
    }
}

static const charset ParseCharSet(const string& s, unsigned& pos)
{
    unsigned b=s.size();
    charset result;
    bool negative=false, begin=true, range_ok=false;
    bool was_range = false;
    charset prev;
    
    while(++pos < b)
    {
        charset key;
        switch(s[pos])
        {
            case ']':
            {
                if(!begin) result |= prev;
                if(negative) result.flip();
                return result;
            }
            case '^':
            {
                if(!begin) goto literal;
                negative=true;
                break;
            }
            case '-':
            {
                if(!range_ok) goto literal;
                if(pos+1 >= b || s[pos+1] == ']') goto literal;
                was_range = true;
                break;
            }
            case '\\':
            {
                key = ParseEscape(s, pos);
                goto operand;
            }
            case '[':
            {
                if(s.substr(pos, 9) == "[:print:]") { pos+=9-1; key = GetPrintMask(); goto operand; }
                if(s.substr(pos, 9) == "[:graph:]") { pos+=9-1; key = GetGraphMask(); goto operand; }
                if(s.substr(pos, 9) == "[:ascii:]") { pos+=9-1; key = GetAsciiMask(); goto operand; }
                if(s.substr(pos, 9) == "[:cntrl:]") { pos+=9-1; key = GetCntrlMask(); goto operand; }
                if(s.substr(pos, 9) == "[:alpha:]") { pos+=9-1; key = GetAlphaMask(); goto operand; }
                if(s.substr(pos, 9) == "[:alnum:]") { pos+=9-1; key = GetAlnumMask(); goto operand; }
                if(s.substr(pos, 9) == "[:lower:]") { pos+=9-1; key = GetLowerMask(); goto operand; }
                if(s.substr(pos, 9) == "[:upper:]") { pos+=9-1; key = GetUpperMask(); goto operand; }
                if(s.substr(pos, 9) == "[:punct:]") { pos+=9-1; key = GetPunctMask(); goto operand; }
                if(s.substr(pos, 9) == "[:space:]") { pos+=9-1; key = GetSpaceMask(); goto operand; }
                if(s.substr(pos, 9) == "[:digit:]") { pos+=9-1; key = GetDecMask(); goto operand; }
                if(s.substr(pos, 8) == "[:word:]")  { pos+=8-1; key = GetWordMask(); goto operand; }
                if(s.substr(pos, 10)== "[:xdigit:]"){ pos+=10-1;key = GetXdigitMask(); goto operand; }
                goto literal;
            }
            default:
            {
            literal:
                key.set((unsigned char)s[pos]);
                goto operand;
            operand:
                if(was_range && key.count() != 1)
                {
                    charset tmp; tmp.set('-');
                    result |= tmp;
                    was_range=false;
                }
                if(was_range)
                {
                    unsigned c1 = prev._Find_first();
                    unsigned c2 = key._Find_first();
                    if(c1 > c2) swap(c1, c2);
                    for(unsigned c=c1; c<=c2; ++c) key.set(c);
                    range_ok=false;
                    was_range=false;
                }
                else
                {
                    range_ok = key.count() == 1;
                    if(!begin) result |= prev;
                    //cout << "key.count(" << key.count() << ") for (" << key << ")\n";
                }
                prev = key;
                break;
            }
        }
        begin=false;
    }
    throw "Unmatched '[' - needs ']'"; // error
    return result;
}

static void ParseCount(const string& s, unsigned& pos, unsigned& min, unsigned& max)
{
    unsigned b=s.size();
    
    unsigned value=0;
    unsigned index=0;
    bool has_value=false;
    
    while(pos+1 < b)
    {
        char c = s[++pos];
        if(c == ',' || c == '}')
        {
            if(has_value) { if(index==0)min=value;else if(index==1)max=value; }
            if(c=='}')
            {
                if(index==0 && has_value) max=min;
                return;
            }
            value=0; has_value=false; ++index;
        }
        else if(isdigit(c))
        {
            value=value*10 + (c-'0');
            has_value=true;
        }
        else
            throw "Invalid character in '{}'"; // error
    }
    throw "Unmatched '{' - needs '}'"; // error
}

static const choices Parse(const string& s, unsigned& pos)
{
    unsigned b=s.size();
    
    bool count_ok = false;
    choices result;
    sequence seq;
    
    for(; pos < b; ++pos)
    {
        charset key;
        switch(s[pos])
        {
            case '(':
            {
                if(s.substr(pos+1,2) == "?:")
                {
                    // ignore
                    pos += 2;
                }
                if(s[pos+1]=='?')
                {
                    std::string escape = s.substr(pos,3);
                    throw std::string("Unexpected '?' escape \"") + escape + "\"";
                }
                struct item ch;
                ++pos;
                ch.tree = new choices(Parse(s, pos));
                seq.push_back(ch);
                count_ok = true;
                if(s[pos] != ')')
                {
                    throw "Unmatched '(' - needs ')'";
                }
                break;
            }
            case ')':
            {
                result.push_back(seq);
                return result;
            }
            case '|':
            {
                result.push_back(seq);
                seq.clear();
                count_ok = false;
                break;
            }
            case '[': // character set
            {
                key = ParseCharSet(s, pos);
                goto gotchar;
            }
            //case ']': throw "Unexpected right bracket"; // not really error - handle as raw.
            case '\\':
            {
                key = ParseEscape(s, pos);
                goto gotchar;
            }
            case '.': // any char but "\n"
            {
                key = GetDotMask();
                goto gotchar;
            }
            case '*': // count: 0-inf
            {
                if(count_ok)
                {
                    struct item& ch = seq.back();
                    ch.min = 0;
                    ch.max = uinf;
                    count_ok = false;
                    goto count_end;
                }
                throw "Unexpected '*'";
                break;
            }
            case '+': // count: 1-inf
            {
                if(count_ok)
                {
                    struct item& ch = seq.back();
                    ch.min = 1;
                    ch.max = uinf;
                    count_ok = false;
                    goto count_end;
                }
                throw "Unexpected '+'";
                break;
            }
            case '?': // count: 0-1
            {
                if(count_ok)
                {
                    struct item& ch = seq.back();
                    ch.min = 0;
                    ch.max = 1;
                    count_ok = false;
                    goto count_end;
                }
                throw "Unexpected '?'";
                break;
            count_end:
                if(pos+1<b && s[pos+1]=='?')
                {
                    struct item& ch = seq.back();
                    ch.greedy = false;
                    ++pos;
                }
                break;
            }
            case '{': // count: "n,m" or "n," or "n" or ",m"
            {
                unsigned n=0;
                unsigned m=uinf;
                
                ParseCount(s, pos, n,m);
                
                if(count_ok)
                {
                    struct item& ch = seq.back();
                    ch.min = n;
                    ch.max = m;
                    count_ok = false;
                    goto count_end;
                }
                throw "Unexpected left bracet";
                break;
            }
            //case '}': throw "Unexpected right bracet"; // not really error - handle as raw.
            case '^': // only string begin
            {
                throw "string-begin '^' not handled, sorry";
                break;
            }
            case '$': // only string end
            {  
                throw "string-end '$' not handled, sorry";
                break;
            }
            default:
            {
                key.set((unsigned char)s[pos]);
                goto gotchar;
            gotchar:
                struct item ch;
                ch.ch  = key;
                seq.push_back(ch);
                count_ok = true;
                break;
            }
        }
    }
    result.push_back(seq);
    OptimizeTree(result);
    return result;
}

const string EscapeChar(unsigned char c)
{
    if(c == '\n') return "\\n";
    if(c == '\r') return "\\r";
    if(c == '\t') return "\\t";
    if(c == '\v') return "\\v";
    if(c == '\f') return "\\f";
    if(c == '\a') return "\\a";
    if(c ==  27)  return "\\e";
    if(c == '\\') return string("\\") + (char)c;
    if(c < 32) return string("\\c") + (char)(c+64);
    if(c >= 0x20 && c <= 0x7E) { string tmp; tmp += c; return tmp; }
    if(c >= 0xA0 && c <= 0xFF) { string tmp; tmp += c; return tmp; }
    char Buf[64];
    sprintf(Buf, "\\%03o", c);
    return Buf;
}

void DumpKey(const charset& s)
{
    if(s == GetDotMask()) { cout << '.'; return; }
    if(s.count() == 1)
    {
        char c = s._Find_first();
        switch(c)
        {
            case '?': case '(': case ')': case '|':
            case '[': case '\\': case '.': case '*':
            case '+': case '{': case '^': case '$': //}
                cout << '\\' << c;
                return;
        }
    }

    string result[2];
    unsigned size[2];
    for(unsigned flip=0; flip<2; ++flip)
    {
        string sets;
        unsigned n=0;
        bool need_set=false;
        
        charset tmp=s;
        if(flip)
        {
            tmp.flip();
            sets += '^';
            need_set=true;
        }
        bool has_circumflex=false;
        bool has_rightbracket=false;

    #if 1
        if((tmp|GetAsciiMask()) == tmp) { ++n;sets += "[:ascii:]"; tmp &= ~GetAsciiMask(); need_set=true; }
        if((tmp|GetPrintMask()) == tmp) { ++n;sets += "[:print:]"; tmp &= ~GetPrintMask(); need_set=true; }
        if((tmp|GetGraphMask()) == tmp) { ++n;sets += "[:graph:]"; tmp &= ~GetGraphMask(); need_set=true; }
        
        if((tmp|GetWordMask()) == tmp)  { ++n;sets += "\\w"; tmp &= ~GetWordMask(); }
        if((tmp|GetAlnumMask()) == tmp) { ++n;sets += "[:alnum:]"; tmp &= ~GetAlnumMask(); need_set=true; }
    /**/
        if((tmp|GetAlphaMask()) == tmp) { ++n;sets += "[:alpha:]"; tmp &= ~GetAlphaMask(); need_set=true; }
    /*
        if((tmp|GetLowerMask()) == tmp) { ++n;sets += "[:lower:]"; tmp &= ~GetLowerMask(); need_set=true; }
        if((tmp|GetUpperMask()) == tmp) { ++n;sets += "[:upper:]"; tmp &= ~GetUpperMask(); need_set=true; }
    */
        if((tmp|GetXdigitMask()) == tmp){ ++n;sets += "[:xdigit:]"; tmp &= ~GetXdigitMask(); need_set=true; }
        if((tmp|GetDecMask()) == tmp)   { ++n;sets += "\\d"; tmp &= ~GetDecMask(); need_set=true; }

        if((tmp|GetPunctMask()) == tmp) { ++n;sets += "[:punct:]"; tmp &= ~GetPunctMask(); need_set=true; }
        if((tmp|GetCntrlMask()) == tmp) { ++n;sets += "[:cntrl:]"; tmp &= ~GetCntrlMask(); need_set=true; }
        if((tmp|GetSpaceMask()) == tmp) { ++n;sets += "[:space:]"; tmp &= ~GetSpaceMask(); }
        if((tmp|GetPSpaceMask()) == tmp){ ++n;sets += "\\s"; tmp &= ~GetPSpaceMask(); }
    #endif
        
        if(tmp['-'] && (!tmp['-'-1] || !tmp['-'+1]))
        {
            //if(n) sets += '\\';
            sets += '-'; ++n;
            tmp.reset('-');
        }
        if(tmp[']'] && (!tmp[']'-1] || !tmp[']'+1]))
        {
            tmp.reset(']');
            has_rightbracket=true;
        }
        if(tmp['^'] && tmp._Find_first() == '^' && sets.empty())
        {
            tmp.reset('^');
            has_circumflex=true;
        }

        unsigned lower=256, prev=256;
        for(unsigned a=0; a<=256; ++a)
            if(a==256 || tmp[a])
            {
                if(a != prev+1 || a == 256)
                {
                    if(lower < 256)
                    {
                        n += prev-lower+1;
                        
                        sets += EscapeChar(lower);
                        
                        if(prev > lower+1) { sets += '-'; need_set = true; }
                        
                        if(lower != prev)
                        {
                            sets += EscapeChar(prev);
                        }
                    }
                    lower=a;
                }
                prev=a;
            }
        
        if(has_circumflex)
        {
            sets += '^';
            ++n;
        }
        if(has_rightbracket)
        {
            if(!n) sets += ']';
            else sets += "\\]";
            ++n;
        }
        
        if(need_set || n > 1) result[flip] += '[';
        result[flip] += sets;
        if(need_set || n > 1) result[flip] += ']';
        size[flip] = n;
    }
    if(size[0] <= size[1])
        cout << result[0];
    else
        cout << result[1];
}

void DumpSequence(const sequence& s)
{
    for(vector<item>::const_iterator
        i = s.begin(); i != s.end(); ++i)
    {
        bool need_parens = i->min!=1 || i->max!=1;
        
        if(i->tree)
            DumpTree(*i->tree, need_parens);
        else
            DumpKey(i->ch);
        
        if(i->min != 1 || i->max != 1)
        {
            if(i->max == uinf)
            {
                if(i->min == 0) cout << '*';
                else if(i->min == 1) cout << '+';
                else cout << '{' << i->min << ",}";
            }
            else if(i->min == 0)
            {
                if(i->max == 1) cout << '?';
                else cout << "{," << i->max << '}';
            }
            else
            {
                if(i->min == i->max)
                {
                    // http looks nicer than ht{2}p
                    // but [[:xdigit:]]{2} is nicer than [[:xdigit:]][[:xdigit:]]
                    if(i->min < 3 && !i->tree && i->ch.count() == 1)
                    {
                        for(unsigned a=1; a<i->min; ++a) DumpKey(i->ch);
                    }
                    else
                        cout << '{' << i->min << '}';
                }
                else
                {
                    cout << '{' << i->min << ',' << i->max << '}';
                }
            }
            if(!i->greedy) cout << '?';
        }
    }
}

void DumpTree(const choices& c, bool need_parens)
{
    if(c.size() != 1) need_parens = true;
    
    if(need_parens) cout << "(?:";

    for(choices::const_iterator
        i = c.begin(); i != c.end(); ++i)
    {
        if(i != c.begin()) cout << "|";
        DumpSequence(*i);
    }
    if(need_parens) cout << ")";
}

static void TestSet(const string& s)
{
    unsigned a=0;
    charset tmp = ParseCharSet("[" + s + "]", a);
    
    cout << "[" << s << "](" << a << "):";
    
    for(unsigned a=0; a<256; ++a)
    {
        if(tmp[a])
            cout << (char)a;
    }
    
    cout << endl;
}

/*
void CompileItem(const item& it, const string& next)
{
    
}
void CompileSequence(const sequence& seq, const string& next)
{
    for(unsigned a=0; a<seq.size(); ++a)
    {
        CompileItem(seq[a], next);
    }
}
void CompileTree(const choices& c, const string& next)
{  
    for(choices::const_iterator
        j, i = c.begin(); i != c.end(); i=j)
    {
        j=i; ++j; 
        string nok = j==c.end() ? next : CreateLabel();
        CompileSequence(*i, nok);
    }
}
*/


int main(int argc, const char* const* argv)
{
    if(argc != 2)
    {
        cout
        << "regex "VERSION" - Copyright (C) 1992,2004 Bisqwit (http://iki.fi/Bisqwit/)\n"
           "This program is distributed under the terms of General Public License.\n"
           "\n"
           "Usage: regex-opt <regexp>\n";;
        return 0;
    }
    try {
    string regex = argv[1];
    unsigned pos=0;
    choices tree = Parse(regex, pos);
    
    DumpTree(tree);

/*
    TestSet("abcdef");
    TestSet("0-9");
    TestSet("ac-km");
    TestSet("a-z0-9");
    TestSet("-abc");
    TestSet("a-c-f");
    TestSet("abc-");
    TestSet("^abc");
    TestSet("a\\d-fh");
    TestSet("a\\143fh");
    TestSet("a\\143-fh");
*/

    }
    catch(const char *s)
    {
        cout << "Error: " << s << endl;
        return -1;
    }
    catch(const std::string& s)
    {
        cout << "Error: " << s.c_str() << endl;
        return -1;
    }
    return 0;
}
