/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include<stdio.h>
#include<stdbool.h>
#include<string.h>
#include"../CRoaring/roaring.hh"
#include"inc/regex.h"
#include<stack>
#include<iostream>
#include<chrono>
template<class NFA_t,class StateSet>
NFA_t Lexer::bracket_expression(const char* start, const char** cp2,uint32_t new_initial,int ps,const char *p,StateSet* states){
    bool escaped=false;
    while(*cp2-start<ps&&!(!(escaped^=(**cp2=='\\'))&&*((*cp2)++)==']'));
    //if(*i==ps) throw std::runtime_error("invalid expression!");
    return NFA_t(new_initial,states_n,states);
}
std::ostream& operator<<(std::ostream& out, PseudoNFA const& pnfa){
    out<<"size: "<<pnfa.size<<std::endl;
    return out;
}
PseudoNFA operator+(PseudoNFA& input,int rotate){
    return PseudoNFA(input.initial_state+rotate,input.size);
}
#define next_initial(stack) (stack.empty()?0:stack.top().initial_state+stack.top().size)
template<class NFA_t,class StateSet>
NFA_t Lexer::build_NFA(const char* p, StateSet* states){
    bool escaped=false;
    std::stack<NFA_t> nfas;
    std::stack<operation> ops;
    ops.push(CONCATENATION);
    int ps=strlen(p);
    const char* cp=p;
    auto clear_stack=[&](){
        NFA_t cur_nfa(std::move(nfas.top())); //FIXME not in printable state
        if(ops.size()){
            ops.pop();
            while(nfas.size()>1&&ops.top()!=BRACKETS){
                if(ops.top()==CONCATENATION){   
                    nfas.pop();
                    ops.pop();
                    nfas.top()*=cur_nfa;
                    cur_nfa=std::move(nfas.top());
                }
                else if(ops.top()==OR){
                    NFA_t intermediate(std::move(cur_nfa));
                    ops.pop();
                    ops.pop();
                    nfas.pop();
                    cur_nfa=std::move(nfas.top());
                    while(nfas.size()>1&&ops.top()==CONCATENATION){
                        nfas.pop();
                        ops.pop();
                        nfas.top()*=cur_nfa;
                        cur_nfa=std::move(nfas.top());
                    }
                    cur_nfa|=intermediate;
                }
            }
        }
        ops.push(CONCATENATION);
        nfas.pop();
        nfas.push(std::move(cur_nfa));
    };
    auto repeat=[&](){
        nfas.push(std::move(nfas.top()+(((int32_t)nfas.top().size))));
        ops.push(CONCATENATION);
    };
    do{
        if(!escaped&&*cp=='\\'){
            escaped=true;
            continue;
        }
        switch((*cp)*(!escaped)) {
            case '[':
                nfas.push(Lexer::bracket_expression<NFA_t>(cp,&cp,nfas.top().initial_state+nfas.top().size,ps-(cp-p),p,states));
                ops.push(CONCATENATION);
                break;
            case '(':
                ops.push(BRACKETS);
                break;
            case '|':
                ops.push(OR);
                break;
            case ')':
                clear_stack();
                break;
            case '*':
                nfas.top()*1;
                break;
            case '+':
                repeat();
                nfas.top()*1;
                break;
            case '?':
                nfas.top()|=NFA_t(next_initial(nfas),states_n,states);
                break;
            case '{':
            {
                char* cn1;
                NFA_t repeated(nfas.top());
                int m=strtol(cp,&cn1,10);
                for(int i=0;i<m;i++) repeat();
                if(*cn1!='}'){
                    char* cn2;
                    int n=strtol(cn1,&cn2,10);
                    if(!n){
                        repeat();
                        nfas.top()*1;
                    } else if(n>m){
                        repeated|=NFA_t(next_initial(nfas),states_n,states);
                        for(int k=m;k<n;k++){
                            nfas.push(repeated);
                            ops.push(CONCATENATION);
                        }
                    }
                }
                break;
            }
            case '^':
            case '$':
                nfas.push(NFA_t(next_initial(nfas),'\0',states_n,states));
                ops.push(CONCATENATION); //states
                break;
            default:
                nfas.push(NFA_t(next_initial(nfas),*cp,states_n,states));
                ops.push(CONCATENATION);
                break;
        }
        escaped=false;
    }while(++cp-p<ps);
    clear_stack();
    if(nfas.size()!=1) throw std::runtime_error("invalid expression");
    NFA_t new_nfa=nfas.top();
    nfas.pop();
    return new_nfa;
}

int main(){
    char* text=NULL;
    char* pattern=NULL;
    size_t len;
    if (getline(&text, &len, stdin) == -1||getline(&pattern, &len, stdin)==-1)
            return -1;
    text[strlen(text)-1]='\0';
    pattern[strlen(pattern)-1]='\0';
    auto start_time = std::chrono::high_resolution_clock::now();
    Lexer l(pattern);
    auto end_time = std::chrono::high_resolution_clock::now();
    std::cout<<"################################# final NFA: "<<std::endl;
    l.exec->print();
    std::cout<<"time: " <<(end_time - start_time)/std::chrono::milliseconds(1) << "ms\n";
    free(text);
    free(pattern);
    return 0;
}

