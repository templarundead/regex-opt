<?php
//TITLE=Perl-compatible regular expression optimizer

$title = 'Perl-compatible regular expression optimizer';
$progname = 'regex-opt';
$git = 'git://bisqwit.iki.fi/regex-opt.git';

$text = array(
   '1. Purpose' => "

Optimizes perl-compatible regular expressions.

", '1. Usage' => "

The general syntax for running the program is:
<code>regex-opt &lt;regexp></code>
<p>
Example:<br>
<code>regex-opt 'xaz|xbz|xcz'<br>
x[a-c]z</code>
<p>

Try running regex-opt on
<a href=\"http://www.foad.org/%7Eabigail/\">Abigail</a>'s 7 kilobyte
<a href=\"http://www.foad.org/~abigail/Perl/url3.regex\">URL regexp</a>.
The result should be about 5 kilobytes long.


", '1. Supported syntax' => "

<ul>
 <li>* (repeat 0-inf)</li>
 <li>+ (repeat 1-inf)</li>
 <li>? (repeat 0-1)</li>
 <li>{n} (repeat n)</li>
 <li>{n,} (repeat n-inf)</li>
 <li>{n,m} (repeat n-m)</li>
 <li>. (accept any char except \\n)</li>
 <li>[a-z] (character sets)</li>
 <li>[^a-z] (inverse character sets)</li>
 <li>[[:alpha:]] (character classes)</li>
 <li>\\s (and other character classes and escapes)</li>
 <li>x|y (alternatives)</li>
 <li>(?:x|y) (non-capturing grouping)</li>
 <li>*? (non-greedy repeat)</li>
</ul>
 
", '1. Unsupported syntax' => "

<ul>
 <li>^ (match string-begin)</li>
 <li>\$ (match string-end)</li>
 <li>() (capturing is converted to noncapturing)</li>
 <li>Any (? -command that is not mentioned in supported syntax</li>
 <li>Unicode-specific markup</li>
</ul>

", '1. Optimizations performed' => "

<ul>
 <li>Character set optimization: [A-Zabcdefgh-yz0-9%] becomes [[:alnum:]%]</li>
 <li>Alternate characters: y|[yp]|[zx] becomes [px-z]</li>
 <li>Counting: aaa* and aa+ become a{2,} and (a?){3} becomes a{0,3}</li>
 <li>Combining: abcde|xycde becomes (?:ab|xy)cde</li>
 <li>Parenthesis reduction: ((abc)) becomes abc, (xx|yy)|zz becomes xx|yy|zz</li>
 <li>Compression: xyzyzxyzyz becomes (?:x(?:yz){2}){2}
  <ul>
   <li>This might not be always a good thing.</li>
  </ul></li>
 <li>Choice counting: a+|aa+ becomes a+, (b|) becomes b?, dxxxxb|dxxxb|dxxb|dxb becomes dx{1,4}b</li>
</ul>

", '1. Optimizations not performed' => "

<ul>
 <li>Combining counts:
  <ul>
   <li>a?|b? should become (?:a|b)?, now becomes a?|b?</li>
  </ul></li>
 <li>Redundancy removal (removal of alternatives that are subsets of other alternatives):
  <ul>
   <li>xfooy|x[a-q]+y should become x[a-q]+y, now becomes x(?:foo|[a-q]+)y</li>
  </ul></li>
</ul>
Help in solving these shortcomings would be welcome.

", '1. Copying' => "

regex-opt has been written by Joel Yliluoma, a.k.a.
<a href=\"http://iki.fi/bisqwit/\">Bisqwit</a>,<br>
and is distributed under the terms of the
<a href=\"http://www.gnu.org/licenses/licenses.html#GPL\">General Public License</a> (GPL).

", '1. Requirements' => "

For compiling you need the following GNU tools: g++, make.<br>

");
include '/WWW/progdesc.php';
