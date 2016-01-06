/**
Copyright (c) 2012-2014 Microsoft Corporation
   
Module Name:

    Symbol.java

Abstract:

Author:

    @author Christoph Wintersteiger (cwinter) 2012-03-15

Notes:
    
**/ 

package com.microsoft.z3;

import com.microsoft.z3.enumerations.Z3_symbol_kind;

/**
 * Symbols are used to name several term and type constructors.
 **/
public class Symbol extends Z3Object
{
    /**
     * The kind of the symbol (int or string)
     **/
    protected Z3_symbol_kind getKind()
    {
        return Z3_symbol_kind.fromInt(Native.getSymbolKind(getContext().nCtx(),
                getNativeObject()));
    }

    /**
     * Indicates whether the symbol is of Int kind
     **/
    public boolean isIntSymbol()
    {
        return getKind() == Z3_symbol_kind.Z3_INT_SYMBOL;
    }

    /**
     * Indicates whether the symbol is of string kind.
     **/
    public boolean isStringSymbol()
    {
        return getKind() == Z3_symbol_kind.Z3_STRING_SYMBOL;
    }

    @Override
    public boolean equals(Object o)
    {
        if (o == null) return false;
        if (o == this) return true;
        if (o.getClass() != this.getClass()) return false;
        Symbol other = (Symbol) o;
        return this.getNativeObject() == other.getNativeObject();
    }

    /**
     * A string representation of the symbol.
     **/
    @Override
    public String toString()
    {
        try
        {
            if (isIntSymbol())
                return Integer.toString(((IntSymbol) this).getInt());
            else if (isStringSymbol())
                return ((StringSymbol) this).getString();
            else
                return new String(
                        "Z3Exception: Unknown symbol kind encountered.");
        } catch (Z3Exception ex)
        {
            return new String("Z3Exception: " + ex.getMessage());
        }
    }

    /**
     * Symbol constructor
     **/
    protected Symbol(Context ctx, long obj)
    {
        super(ctx, obj);
    }

    static Symbol create(Context ctx, long obj)
    {
        switch (Z3_symbol_kind.fromInt(Native.getSymbolKind(ctx.nCtx(), obj)))
        {
        case Z3_INT_SYMBOL:
            return new IntSymbol(ctx, obj);
        case Z3_STRING_SYMBOL:
            return new StringSymbol(ctx, obj);
        default:
            throw new Z3Exception("Unknown symbol kind encountered");
        }
    }
}
