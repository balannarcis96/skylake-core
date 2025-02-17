# skylake-core
[Clang19+; c++23; linux; x86-64] - Specialized core library.

## Goals:
1. Maximize compilation speed
   - Absolute minimal dependencies
   - 100% Specialized for:
       <table>
         <tr>
           <td>compiler</td>
           <td><b>clang 19+</b></td>
         </tr>
         <tr>
           <td>c++ version</td>
           <td><b>c++23</b></td>
         </tr>
         <tr>
           <td>os</td>
           <td><b>linux (dev os: debian 2025-01-02)</b></td>
         </tr>
         <tr>
           <td>arch</td>
           <td><b>x86-64</b></td>
         </tr>
       </table>
   - use compiler builtins as much as possible
2. Simple and fast
