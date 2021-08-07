# Perl-compatible regular expression optimizer ## 0\. Contents This is the documentation of regex-opt-1.2.2.

<div class="toc">

<table cellspacing="0" cellpadding="0" class="toc">

<tbody>

<tr>

<td width="50%" valign="middle" align="left" nowrap="" class="toc">   1\. [Purpose](#h0)    2\. [Usage](#h1)    3\. [Supported syntax](#h2)    4\. [Unsupported syntax](#h3)</td>

<td width="50%" valign="middle" align="left" nowrap="" class="toc">   5\. [Optimizations performed](#h4)    6\. [Optimizations not performed](#h5)    7\. [Copying](#h6)    8\. [Requirements](#h7)    9\. [Downloading](#download)</td>

</tr>

</tbody>

</table>

</div>

## <a name="h0"></a>1\. Purpose

<div class="level2" id="divh0">Optimizes perl-compatible regular expressions.</div>

## <a name="h1"></a>2\. Usage

<div class="level2" id="divh1">The general syntax for running the program is: `regex-opt <regexp>` Example: `regex-opt 'xaz|xbz|xcz' x[a-c]z` Try running regex-opt on [Abigail](http://www.foad.org/%7Eabigail/)'s 7 kilobyte [URL regexp](http://www.foad.org/~abigail/Perl/url3.regex). The result should be about 5 kilobytes long.</regexp></div>

## <a name="h2"></a>3\. Supported syntax

<div class="level2" id="divh2">* * (repeat 0-inf) * + (repeat 1-inf) * ? (repeat 0-1) * {n} (repeat n) * {n,} (repeat n-inf) * {n,m} (repeat n-m) * . (accept any char except \n) * [a-z] (character sets) * [^a-z] (inverse character sets) * [[:alpha:]] (character classes) * \s (and other character classes and escapes) * x|y (alternatives) * (?:x|y) (non-capturing grouping) * *? (non-greedy repeat)</div>

## <a name="h3"></a>4\. Unsupported syntax

<div class="level2" id="divh3">* ^ (match string-begin) * $ (match string-end) * () (capturing is converted to noncapturing) * Any (? -command that is not mentioned in supported syntax * Unicode-specific markup</div>

## <a name="h4"></a>5\. Optimizations performed

<div class="level2" id="divh4">* Character set optimization: [A-Zabcdefgh-yz0-9%] becomes [[:alnum:]%] * Alternate characters: y|[yp]|[zx] becomes [px-z] * Counting: aaa* and aa+ become a{2,} and (a?){3} becomes a{0,3} * Combining: abcde|xycde becomes (?:ab|xy)cde * Parenthesis reduction: ((abc)) becomes abc, (xx|yy)|zz becomes xx|yy|zz * Compression: xyzyzxyzyz becomes (?:x(?:yz){2}){2} * This might not be always a good thing. * Choice counting: a+|aa+ becomes a+, (b|) becomes b?, dxxxxb|dxxxb|dxxb|dxb becomes dx{1,4}b</div>

## <a name="h5"></a>6\. Optimizations not performed

<div class="level2" id="divh5">* Combining counts: * a?|b? should become (?:a|b)?, now becomes a?|b? * Redundancy removal (removal of alternatives that are subsets of other alternatives): * xfooy|x[a-q]+y should become x[a-q]+y, now becomes x(?:foo|[a-q]+)y Help in solving these shortcomings would be welcome.</div>

## <a name="h6"></a>7\. Copying

<div class="level2" id="divh6">regex-opt has been written by Joel Yliluoma, a.k.a. [Bisqwit](http://iki.fi/bisqwit/), and is distributed under the terms of the [General Public License](http://www.gnu.org/licenses/licenses.html#GPL) (GPL).</div>

## <a name="h7"></a>8\. Requirements

<div class="level2" id="divh7">For compiling you need the following GNU tools: g++, make.</div>

## <a name="download"></a>9\. Downloading

<div class="level2" id="divdownload">The official home page of regex-opt is at [http://iki.fi/bisqwit/source/regex-opt.html](http://iki.fi/bisqwit/source/regex-opt.html). Check there for new versions.</div>

<small>Generated from <tt>progdesc.php</tt> (last updated: Wed, 21 Feb 2007 17:27:15 +0200) with <tt>docmaker.php</tt> (last updated: Sun, 12 Jun 2005 06:08:02 +0300) at Tue, 27 Feb 2007 16:46:38 +0200</small>