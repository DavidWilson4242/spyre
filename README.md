<h1>Spyre Programming Langage</h1>
Spyre is a hobbyist programming language.

<h3>Key features</h3>
<ul>
  <li>Statically typed</li>
  <li>Interpreted</li>
  <li>Garbage collected</li>
  <li>Procedural</li>
</ul>

<h3>Installation</h3>
<pre><code>git clone https://DavidWilson4242/spyre.git
cd spyre
make spyre
</code></pre>

<h3>Usage</h3>
<h4>Compiling</h4>
<pre><code>spyre -c inputfile.spy -o outputfile.spyb</code></pre>
<h4>Assembling from instruction file</h4>
<pre><code>spyre -a inputfile.spys -o outputfile.spyb</code></pre>
<h4>Running from bytecode</h4>
<pre><code>spyre -r inputfile.spyb</code></pre>
<h4>Just run my code!</h4>
<pre><code>spyre inputfile.spy</code></pre>

<h3>Compilation Steps</h3>
<ul>
  <li>Lex</li>
  <ul><li>Convert input file into a list of tokens</li></ul>
  <li>Parse</li>
  <li>Typecheck</li>
  <li>Generate</li>
  <li>Assemble</li>
  <li>Execute</li>
</ul>
