# Flow VM Proof-of-concept
# (c) 2013 Tiago Rezende
#
#

TYPE_LIST = 0
TYPE_SYMBOL = 1
TYPE_CODE = 2
TYPE_STRING = 3
TYPE_INT = 4
TYPE_FLOAT = 5

def l1st(code):
    return code[0][1]
def t1st(code):
    return code[0][0]
def lnxt(code):
    return code[1][1]
def tnxt(code):
    return code[1][0]

def push(t,f,rest):
    return ((t, f), (TYPE_LIST, rest))

def llist(l, *idxs):
    if (len(idxs) == 1):
        return l[idxs[0]]
    else:
        return llist(l[idxs[0]],idxs[1:])
def swap_op(code, args, ret, ctx):
    arg0 = args[0]
    arg1 = args[1][0]
    return (lnxt(code),(arg1,(TYPE_LIST,(arg0,args[1][1]))),ret,ctx)
ops = {
    pushk: lambda(code, args, ret, ctx) (lnxt(lnxt(code)),
                                        push(t1st(lnxt(code)),l1st(t1st(code)),args),
                                        ret,
                                        ctx),
    jump: lambda(code, args, ret, ctx) (l1st(args),
                                        lnxt(args),
                                        ret,
                                        ctx),
    ret: lambda(code, args, ret, ctx) (l1st(ret),
                                       args,
                                       lnxt(ret),
                                       ctx),
    call: lambda(code, args, ret, ctx) (l1st(lnxt(code)),
                                        args,
                                        push(TYPE_CODE,lnxt(lnxt(code)),ret),
                                        ctx),
    call_a: lambda(code, args, ret, ctx) (l1st(args),
                                          lnxt(args),
                                          push(TYPE_CODE,lnxt(code),ret),
                                          ctx),
    #callp: lambda(code, args, ret, ctx)
    #callp_a:
    #cons:
    drop: lambda(code, args, ret, ctx) (lnxt(code),
                                        lnxt(args),
                                        ret,
                                        ctx),
    dup: lambda(code, args, ret, ctx) (lnxt(code),
                                       push(t1st(args),l1st(args),args),
                                       ret,
                                       ctx),
    swap: swap_op,
    dup: lambda(code, args, ret, ctx) (lnxt(code),
                                       push(t1st(args),l1st(args),
                                            push(t1st(lnxt(args)),l1st(lnxt(args)),args)),
                                       ret,
                                       ctx),
    }
def make_node(first_type, first, rest_type, rest):
    return ((first_type, first), (rest_type, rest))

def interpret(code, ctx):
    args = None
    ret = None
    while (code not None) and (t1st(code) == TYPE_CODE):
        code, args, ret, ctx = ops[l1st(code)](code, args, ret, ctx)
