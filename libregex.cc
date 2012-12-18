#include <string>
#include <bitset>
#include <cctype>
#include <algorithm>
#include <iostream>

#include "libregex.hh"

/* Extended regular expression optimizer */
/* Copyright (C) 1992,2006 Bisqwit (http://iki.fi/bisqwit/) */

static const unsigned uinf = ~0U;

typedef regexopt_charset charset;
typedef regexopt_sequence sequence;
typedef regexopt_choices choices;
typedef regexopt_item item;

#include "rangeset.hh"

bool regexopt_item::is_equal(const regexopt_item& b) const
{
    if(greedy != b.greedy) return false;
    if(tree) return b.tree && *tree == *b.tree;
    if(b.tree) return false;
    return ch == b.ch;
}

bool regexopt_item::operator==(const regexopt_item& b) const
{
    if(!tree != !b.tree
    || min != b.min
    || max != b.max
    || greedy != b.greedy) return false;
    if(tree) { return *tree == *b.tree; }
    return ch == b.ch;
}


enum ParensFlag { no_parens=false, yes_parens=true, automatic=2 };

static void DumpTree(std::ostream&, const choices& c, ParensFlag need_parens=automatic);
static void DumpSequence(std::ostream&, const sequence& s);
static void DumpKey(std::ostream&, const charset& s);

static void OptimizeSequence(sequence& seq);
static void OptimizeTree(choices& tree);

static void SumCounts(unsigned& target, unsigned value)
{
    if(target == uinf) return;
    if(value == uinf)
        target = value;
    else
        target += value;
}

static unsigned CountEqualSequenceAtBegin(const sequence& a, const sequence& b, unsigned max=0)
{
    unsigned c=0, as = a.size(), bs = b.size(), ms = std::min(as, bs);
    if(!max) max=ms; else max = std::min(max, ms);
    while(c < max && a[c] == b[c]) ++c;
    return c;
}
static unsigned CountEqualSequenceAtEnd(const sequence& a, const sequence& b, unsigned max=0)
{
    unsigned c=0, as = a.size(), bs = b.size(), ms = std::min(as, bs);
    if(!max) max=ms; else max = std::min(max, ms);
    while(c < max && a[as-c-1] == b[bs-c-1]) ++c;
    return c;
}
static bool IsEqualSequenceAtBegin(const sequence& a, const sequence& b, unsigned min_length)
{
    return CountEqualSequenceAtBegin(a, b, min_length) >= min_length;
}
static bool IsEqualSequenceAtEnd(const sequence& a, const sequence& b, unsigned min_length)
{
    return CountEqualSequenceAtEnd(a, b, min_length) >= min_length;
}
static const sequence CopySequenceFromEnd(const sequence& a, unsigned n)
{
    sequence result;
    result.insert(result.end(), a.end() - std::min(n, (unsigned)a.size()), a.end());
    return result;
}
static const sequence CopySequenceFromBegin(const sequence& a, unsigned n)
{
    sequence result;
    result.insert(result.end(), a.begin(), a.begin() + std::min(n, (unsigned)a.size()));
    return result;
}
static void RemoveSequenceFromBegin(sequence& a, unsigned n)
{
    a.erase(a.begin(), a.begin() + std::min((unsigned)a.size(), n));
}
static void RemoveSequenceFromEnd(sequence& a, unsigned n)
{
    a.erase(a.end() - std::min(n, (unsigned)a.size()), a.end());
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
        item& it = seq[a];

        if(it.max==0)
            erased=true;
        else if(it.is_equal(seq[prev]))
        {
            SumCounts(seq[prev].min, it.min);
            SumCounts(seq[prev].max, it.max);
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

static void DeleteEmptyNodesInSequence(sequence& seq)
{
    for(unsigned a=0; a<seq.size(); ++a)
    {
    redo:
        item& it = seq[a];

        /* Delete empty nodes like ()? or []+ */
        if((it.tree  && it.tree->empty())
        || (!it.tree && !it.ch.any()))
        {
            seq.erase(seq.begin()+a);
            if(a >= seq.size())break;
            goto redo;
        }
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
        else if(seq[a].max > 0
             && ((seq[a].tree && seq[a].tree->size() > 0)
              || (!seq[a].tree && seq[a].ch.any())))
        {
            result.push_back(seq[a]);
        }
    }
    seq = result;
}

static void OptimizeSequence(sequence& seq)
{
    for(sequence::iterator i=seq.begin(); i!=seq.end(); ++i)
    {
        i->Optimize();
    }
    FlattenSequence(seq);
    CompressSequence(seq);
    /*
    DeleteEmptyNodesInSequence(seq);
    */
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

        if(it.tree && it.min == 1 && it.max == 1)
        {
            tree.insert(j, it.tree->begin(), it.tree->end());
            tree.erase(i);
        }
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
        if(it.min != 1 || it.max != 1) continue;
        if(it.tree)
        {
            /*if(it.tree->empty())
            {
                found_sets = true;
                tree.erase(i);
            }*/
            continue;
        }

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

static bool CountingCombineTree(choices& tree)
{
    bool did_changes = false;

    /* For each choice, create a map of how many alternatives
     * there are for the first item.
     */
redo_combine:
    for(choices::iterator i=tree.begin(); i!=tree.end(); ++i)
    {
        if(i->size() != 1) continue;
        const item& i_ref = *i->begin();

        rangeset<unsigned> ranges;

        /* In rangeset, the "upper" is exclusive.
         * In choice, the "max" is inclusive.
         * Therefore, convert.
         */
        unsigned i_max = i_ref.max; if(i_max != uinf) ++i_max;
        ranges.set(i_ref.min, i_max);

        for(choices::iterator j=tree.begin(); j!=tree.end(); ++j)
        {
            if(j->empty())
            {
                // If the sequence is empty, it is acceptable to
                // interpret it as a 0-size sequence of anything
                ranges.set(0,1);
                continue;
            }
            if(i == j || j->size() != 1) continue;
            const item& j_ref = *j->begin();

            if(!i_ref.is_equal(j_ref)) continue;

            unsigned j_max = j_ref.max; if(j_max != uinf) ++j_max;
            ranges.set(j_ref.min, j_max);
        }

        /* For each of the combined ranges, find each element
         * that belongs to this particular group. */

        bool changed = false;
        for(rangeset<unsigned>::const_iterator
            ri = ranges.begin(); ri != ranges.end(); ++ri)
        {
            unsigned r_min = ri->lower;
            unsigned r_max = ri->upper; if(r_max != uinf) --r_max;

            bool first = true;
            bool found_something = false;
            for(choices::iterator jnext,j=tree.begin(); j!=tree.end(); j=jnext)
            {
                jnext=j; ++jnext;

                if(j->size() != 1)continue;

                item& j_ref = *j->begin();
                if(!i_ref.is_equal(j_ref)) continue;

                /* If this item is entirely swallowed by
                 * this particular range, assimilate it
                 */

                if(j_ref.min >= r_min && j_ref.max <= r_max)
                {
                    found_something = true;

                    if(first)
                    {
                        first = false;
                        if(j_ref.min == r_min
                        && j_ref.max == r_max) { continue; /* nothing changed */ }

                        /* Update the range and set changed-flag */
                        j_ref.min = r_min;
                        j_ref.max = r_max;
                        changed = true;
                    }
                    else
                    {
                        /* Delete this item and set changed-flag */
                        changed = true;
                        tree.erase(j);
                    }
                }
            }

            if(r_min == 0 && found_something)
            {
                // Then we may delete zero-length sequences
                for(choices::iterator jnext,j=tree.begin(); j!=tree.end(); j=jnext)
                {
                    jnext=j; ++jnext;
                    if(j->empty()) tree.erase(j);
                }
            }
        }
        if(changed)
        {
            did_changes = true;
            goto redo_combine;
        }
    }

    return did_changes;
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

    // Combine those that have common ending
    for(choices::iterator i=tree.begin(); i!=tree.end(); ++i)
    {
        std::list<choices::iterator> com;
        com.push_back(i);
        for(choices::iterator j=i; ++j!=tree.end(); )
        {
            if(IsEqualSequenceAtEnd(*i, *j, 1)) com.push_back(j);
        }
        if(com.size() > 1)
        {
            sequence rep = CopySequenceFromEnd(*i, 1); // Copy the common part
            choices subchoice;

            bool has_empty = false;
            for(std::list<choices::iterator>::iterator
                k = com.begin(); k != com.end(); ++k)
            {
                choices::iterator& ki = *k;
                sequence& kik = *ki;

                RemoveSequenceFromEnd(kik, 1); // Remove the common part
                if(kik.empty())
                    has_empty = true;
                else
                    subchoice.push_back(kik);
                tree.erase(ki);
            }
            if(!subchoice.empty())
            {
                item it;
                it.tree = new choices(subchoice);
                if(has_empty) { it.min=0; } // make it optional
                it.Optimize();
                rep.insert(rep.begin(), it);
                // Insert the new tree into the beginning of the new choice
            }
            tree.push_back(rep);
            return true;
        }
    }

    // Combine those that have common beginning
    // This loop is equal to the previous, except
    // that Begin/begin/back and End/end/front are swapped.
    for(choices::iterator i=tree.begin(); i!=tree.end(); ++i)
    {
        std::list<choices::iterator> com;
        com.push_back(i);
        for(choices::iterator j=i; ++j!=tree.end(); )
        {
            if(IsEqualSequenceAtBegin(*i, *j, 1)) com.push_back(j);
        }
        if(com.size() > 1)
        {
            sequence rep = CopySequenceFromBegin(*i, 1);
            choices subchoice;
            bool has_empty = false;
            for(std::list<choices::iterator>::iterator
                k = com.begin(); k != com.end(); ++k)
            {
                choices::iterator& ki = *k;
                sequence& kik = *ki;

                RemoveSequenceFromBegin(kik, 1);
                if(kik.empty())
                    has_empty = true;
                else
                    subchoice.push_back(kik);
                tree.erase(ki);
            }
            if(!subchoice.empty())
            {
                item it;
                it.tree = new choices(subchoice);
                if(has_empty) { it.min=0; } // make it optional
                it.Optimize();
                rep.insert(rep.end(), it);
                // Insert the new tree into the end of the new choice
            }
            tree.push_back(rep);
            return true;
        }
    }
    return false;
}

static void OptimizeTree(choices& tree)
{
    for(;;)
    {
        for(choices::iterator i=tree.begin(); i!=tree.end(); ++i)
            OptimizeSequence(*i);

        FlattenTree(tree);
        CharsetCombineTree(tree);

        bool changed = CombineTree(tree) || CountingCombineTree(tree);
        if(!changed) break;

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

        // Do convert (x{0,n}){m} to x{0,n*m}

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
                else if(it.min==0 && min==max)
                {
                    choices* tmp = tree;
                    min = 0;
                    max *= it.max;
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
    for(unsigned a='0'; a<='9'; ++a) result.set(a);
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

static const charset ParseEscape(const std::string& s, unsigned& pos)
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

static const charset ParseCharSet(const std::string& s, unsigned& pos)
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
                    if(c1 > c2) std::swap(c1, c2);
                    for(unsigned c=c1; c<=c2; ++c) key.set(c);
                    range_ok=false;
                    was_range=false;
                }
                else
                {
                    range_ok = key.count() == 1;
                    if(!begin) result |= prev;
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

static void ParseCount(const std::string& s, unsigned& pos, unsigned& min, unsigned& max)
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
        {
            std::cerr << "Invalid character: '" << (char)c << "'\n";
            throw "Invalid character in '{}'"; // error
        }
    }
    throw "Unmatched '{' - needs '}'"; // error
}

static const choices Parse(const std::string& s, unsigned& pos)
{
    unsigned b=s.size();

    bool count_ok = false;
    choices result;
    sequence seq;

    bool has_empty = false;

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
                regexopt_item ch;
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
                goto fin;
            }
            case '|':
            {
                if(seq.empty()) has_empty = true; else result.push_back(seq);
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
                    regexopt_item& ch = seq.back();
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
                    regexopt_item& ch = seq.back();
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
                    regexopt_item& ch = seq.back();
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
                    regexopt_item& ch = seq.back();
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
                    regexopt_item& ch = seq.back();
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
                regexopt_item ch;
                ch.ch  = key;
                seq.push_back(ch);
                count_ok = true;
                break;
            }
        }
    }
fin:
    if(seq.empty()) has_empty = true; else result.push_back(seq);
    if(has_empty && !result.empty()) result.push_back(sequence());
    OptimizeTree(result);
    return result;
}

static const std::string EscapeChar(unsigned char c)
{
    if(c == '\n') return "\\n";
    if(c == '\r') return "\\r";
    if(c == '\t') return "\\t";
    if(c == '\v') return "\\v";
    if(c == '\f') return "\\f";
    if(c == '\a') return "\\a";
    if(c ==  27)  return "\\e";
    if(c == '\\') return std::string("\\") + (char)c;
    if(c < 32) return std::string("\\c") + (char)(c+64);
    if(c >= 0x20 && c <= 0x7E   ) { return std::string(1,char(c)); }
    if(c >= 0xA0/*&& c <= 0xFF*/) { return std::string(1,char(c)); }
    char Buf[64];
    std::sprintf(Buf, "\\%03o", c);
    return Buf;
}

static void DumpKey(std::ostream& out, const charset& s)
{
    if(s == GetDotMask()) { out << '.'; return; }
    if(s.count() == 1)
    {
        char c = s._Find_first();
        switch(c)
        {
            case '?': case '(': case ')': case '|':
            case '[': case '\\': case '.': case '*':
            case '+': case '{': case '^': case '$': //}
                out << '\\' << c;
                return;
        }
    }

    std::string result[2];
    unsigned size[2];
    for(unsigned flip=0; flip<2; ++flip)
    {
        std::string sets;
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

        // Put dash ('-') first,
        // unless
        // - It is part of a range
        // - One of these sets is included (which include '-')
        //       ascii, graph, print, punct.
        if(tmp['-']
        && (!tmp['-'-1] || !tmp['-'+1])
        && !((tmp|GetAsciiMask()) == tmp)
        && !((tmp|GetGraphMask()) == tmp)
        && !((tmp|GetPrintMask()) == tmp)
        && !((tmp|GetPunctMask()) == tmp)
          )
        {
            //if(n) sets += '\\';
            sets += '-'; ++n;
            tmp.reset('-');
        }

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

        // Second check for '-'. This should be unnecessary.
        if(tmp['-']
        && (!tmp['-'-1] || !tmp['-'+1])
          )
        {
            if(n) sets += '\\';
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

    if(size[0]==0 && size[1] > 0) size[0] = 255;
    if(size[1]==0 && size[0] > 0) size[1] = 255;

    if(size[0] <= size[1])
        out << result[0];
    else
        out << result[1];
}

static void DumpSequence(std::ostream& out, const sequence& s)
{
    for(std::vector<item>::const_iterator
        i = s.begin(); i != s.end(); ++i)
    {
        ParensFlag need_parens = (i->min!=1 || i->max!=1) ? yes_parens : automatic;

        if(i->tree)
            DumpTree(out, *i->tree, need_parens);
        else
            DumpKey(out, i->ch);

        if(i->min != 1 || i->max != 1)
        {
            if(i->max == uinf)
            {
                if(i->min == 0) out << '*';
                else if(i->min == 1) out << '+';
                else out << '{' << i->min << ",}";
            }
            else if(i->min == 0 && i->max == 1)
            {
                out << '?';
            }
            else
            {
                if(i->min == i->max)
                {
                    // http looks nicer than ht{2}p
                    // but [[:xdigit:]]{2} is nicer than [[:xdigit:]][[:xdigit:]]
                    if(i->min < 3 && !i->tree && i->ch.count() == 1)
                    {
                        for(unsigned a=1; a<i->min; ++a) DumpKey(out, i->ch);
                    }
                    else
                        out << '{' << i->min << '}';
                }
                else
                {
                    out << '{' << i->min << ',' << i->max << '}';
                }
            }
            if(!i->greedy) out << '?';
        }
    }
}

static void DumpTree(std::ostream& out, const choices& c, ParensFlag need_parens)
{
    if(need_parens == automatic)
        need_parens = (ParensFlag)(c.size() != 1);

    if(need_parens) out << "(?:";

    bool first=true;
    for(choices::const_iterator
        i = c.begin(); i != c.end(); ++i)
    {
        if(first)first=false; else out << '|';
        DumpSequence(out, *i);
    }
    if(need_parens) out << ")";
}

void DumpRegexOptTree(std::ostream& out, const regexopt_choices& tree)
{
    DumpTree(out, tree, (ParensFlag)false);
}

static void TestSet(const std::string& s)
{
    unsigned a=0;
    charset tmp = ParseCharSet("[" + s + "]", a);

    std::cout << "[" << s << "](" << a << "):";

    for(unsigned a=0; a<256; ++a)
    {
        if(tmp[a])
            std::cout << (char)a;
    }

    std::cout << std::endl;
}

const regexopt_choices RegexOptParse(const std::string& s, unsigned& pos)
{
    return Parse(s, pos);
}
