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
<pre><code>git clone https://github.com/DavidWilson4242/spyre.git
cd spyre
make spyre
</code></pre>

<h3>Usage</h3>
<h4>Compiling</h4>
<pre><code>spyre -c inputfile.spy -o outputfile.spys</code></pre>
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
  <ul><li>Convert tokens into an abstract syntax tree (AST), validate syntax</li></ul>
  <li>Typecheck</li>
  <ul><li>Resolve types of every expression, ensure no type mistakes</li></ul>
  <li>Generate</li>
  <ul><li>Create an assembly file of Spyre instructions from the AST</li></ul>
  <li>Assemble</li>
  <ul><li>Convert assembly file into raw bytecode that the interpreter can understand</li></ul>
  <li>Execute</li>
  <ul><li>Load libraries, init virtual machine, execute raw bytecode</li></ul>
</ul>
