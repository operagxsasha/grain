open Grain;
open Compile;
open Grain_parsing;
open Grain_utils;

module Doc = Res_doc;

let get_raw_pos_info = (pos: Lexing.position) => (
  pos.pos_fname,
  pos.pos_lnum,
  pos.pos_cnum - pos.pos_bol,
  pos.pos_bol,
);

let getOriginalCode =
    (location: Grain_parsing.Location.t, source: list(string)) => {
  let (_, startline, startc, _) = get_raw_pos_info(location.loc_start);
  let (_, endline, endc, _) = get_raw_pos_info(location.loc_end);

  let text = ref("");
  if (List.length(source) > endline - 1) {
    if (startline == endline) {
      let full_line = List.nth(source, startline - 1);

      let without_trailing = Str.string_before(full_line, endc);

      text := text^ ++ without_trailing;
    } else {
      for (line in startline - 1 to endline - 1) {
        if (line + 1 == startline) {
          text :=
            text^ ++ Str.string_after(List.nth(source, line), startc) ++ "\n"; // What about Windows?
        } else if (line + 1 == endline) {
          text := text^ ++ List.nth(source, line);
        } else {
          text :=
            text^ ++ Str.string_before(List.nth(source, line), endc) ++ "\n"; // What about Windows?
        };
      };
    };
    text^;
  } else {
    "/* Formatter error*/";
  };
};

let lastFileLoc = (): Grain_parsing.Location.t => {
  let endpos: Stdlib__lexing.position = {
    pos_fname: "",
    pos_lnum: max_int,
    pos_cnum: max_int,
    pos_bol: 0,
  };

  let endLoc: Grain_parsing.Location.t = {
    loc_start: endpos,
    loc_end: endpos,
    loc_ghost: false,
  };
  endLoc;
};

let getCommentLoc = (comment: Parsetree.comment) =>
  switch (comment) {
  | Line(cmt) => cmt.cmt_loc
  | Block(cmt) => cmt.cmt_loc
  | Doc(cmt) => cmt.cmt_loc
  | Shebang(cmt) => cmt.cmt_loc
  };

let makeLineStart = (loc: Grain_parsing.Location.t) => {
  let startpos: Stdlib__lexing.position = {
    pos_fname: "",
    pos_lnum: loc.loc_start.pos_lnum,
    pos_cnum: 0,
    pos_bol: 0,
  };

  let mergedLoc: Grain_parsing.Location.t = {
    loc_start: startpos,
    loc_end: loc.loc_start,
    loc_ghost: loc.loc_ghost,
  };
  mergedLoc;
};

let makeStartLoc = (loc: Grain_parsing.Location.t) => {
  let mergedLoc: Grain_parsing.Location.t = {
    loc_start: loc.loc_start,
    loc_end: loc.loc_start,
    loc_ghost: loc.loc_ghost,
  };
  mergedLoc;
};

let makeEndLoc = (loc: Grain_parsing.Location.t) => {
  let mergedLoc: Grain_parsing.Location.t = {
    loc_start: loc.loc_end,
    loc_end: loc.loc_end,
    loc_ghost: loc.loc_ghost,
  };
  mergedLoc;
};

let getLocLine = (loc: Grain_parsing.Location.t) => {
  let (_, line, _, _) = get_raw_pos_info(loc.loc_start);
  line;
};

let getEndLocLine = (loc: Grain_parsing.Location.t) => {
  let (_, line, _, _) = get_raw_pos_info(loc.loc_end);
  line;
};

let commentToDoc = (comment: Parsetree.comment) => {
  let cmtText =
    switch (comment) {
    | Line(cmt) => cmt.cmt_source
    | Block(cmt) => cmt.cmt_source
    | Doc(cmt) => cmt.cmt_source
    | Shebang(cmt) => cmt.cmt_source
    };

  Doc.text(String.trim(cmtText));
};

let isDisableFormattingComment = (comment: Parsetree.comment) => {
  let cmtText =
    switch (comment) {
    | Line(cmt) => cmt.cmt_source
    | Block(cmt) => cmt.cmt_source
    | Doc(cmt) => cmt.cmt_source
    | Shebang(cmt) => cmt.cmt_source
    };

  if (String.trim(cmtText) == "// formatter-ignore") {
    true;
  } else {
    false;
  };
};

let split_comments =
    (comments: list(Grain_parsing__Parsetree.comment), line: int)
    : (
        list(Grain_parsing__Parsetree.comment),
        list(Grain_parsing__Parsetree.comment),
      ) => {
  List.fold_left(
    (acc, c) =>
      if (getLocLine(getCommentLoc(c)) == line) {
        let (thisline, below) = acc;
        (thisline @ [c], below);
      } else {
        let (thisline, below) = acc;
        (thisline, below @ [c]);
      },
    ([], []),
    comments,
  );
};

let print_leading_comments = (comments: list(Parsetree.comment), line: int) => {
  let prevLine = ref(line);
  let prevComment: ref(option(Parsetree.comment)) = ref(None);

  let linesOfComments =
    Doc.join(
      Doc.hardLine,
      List.map(
        c => {
          let nextLine = getLocLine(getCommentLoc(c));
          let ret =
            if (nextLine > prevLine^ + 1) {
              Doc.concat([Doc.hardLine, commentToDoc(c)]);
            } else {
              commentToDoc(c);
            };

          prevLine := getEndLocLine(getCommentLoc(c));
          prevComment := Some(c);
          ret;
        },
        comments,
      ),
    );

  let lastCommentLine =
    switch (prevComment^) {
    | None => line
    | Some(c) => getEndLocLine(getCommentLoc(c))
    };
  (Doc.concat([linesOfComments, Doc.hardLine]), lastCommentLine);
};

let print_multi_comments_raw =
    (comments: list(Parsetree.comment), line: int, leadSpace) => {
  let prevLine = ref(line);
  let prevComment: ref(option(Parsetree.comment)) = ref(None);

  let linesOfComments =
    Doc.join(
      Doc.space,
      List.map(
        c => {
          let nextLine = getLocLine(getCommentLoc(c));
          let ret =
            if (nextLine > prevLine^) {
              if (nextLine > prevLine^ + 1) {
                Doc.concat([Doc.hardLine, Doc.hardLine, commentToDoc(c)]);
              } else {
                Doc.concat([Doc.hardLine, commentToDoc(c)]);
              };
            } else {
              commentToDoc(c);
            };
          prevLine := getEndLocLine(getCommentLoc(c));
          prevComment := Some(c);
          ret;
        },
        comments,
      ),
    );

  Doc.concat([
    leadSpace,
    linesOfComments,
    switch (prevComment^) {
    | Some(Line(_)) => Doc.nil
    | _ => Doc.nil
    },
  ]);
};

let print_multi_comments = (comments: list(Parsetree.comment), line: int) =>
  if (List.length(comments) > 0) {
    let firstComment = List.hd(comments);
    let firstCommentLine = getLocLine(getCommentLoc(firstComment));

    let leadSpace =
      if (firstCommentLine > line) {
        Doc.nil;
      } else {
        Doc.space;
      };
    print_multi_comments_raw(comments, line, leadSpace);
  } else {
    Doc.nil;
  };

let print_multi_comments_no_space =
    (comments: list(Parsetree.comment), line: int) =>
  if (List.length(comments) > 0) {
    let leadSpace = Doc.nil;
    print_multi_comments_raw(comments, line, leadSpace);
  } else {
    Doc.nil;
  };

type stmtList =
  | BlankLine
  | BlockComment(string)
  | Statement(Parsetree.toplevel_stmt);

type sugaredListItem =
  | Regular(Grain_parsing.Parsetree.expression)
  | Spread(Grain_parsing.Parsetree.expression);

type sugaredPatternItem =
  | RegularPattern(Grain_parsing.Parsetree.pattern)
  | SpreadPattern(Grain_parsing.Parsetree.pattern);

let addParens = doc =>
  Doc.concat([
    Doc.lparen,
    Doc.indent(Doc.concat([Doc.softLine, doc])),
    Doc.softLine,
    Doc.rparen,
  ]);

let addBraces = doc =>
  Doc.concat([
    Doc.lbrace,
    Doc.indent(Doc.concat([Doc.softLine, doc])),
    Doc.softLine,
    Doc.rbrace,
  ]);

let infixop = (op: string) => {
  switch (op) {
  | "+"
  | "-"
  | "*"
  | "/"
  | "%"
  | "is"
  | "isnt"
  | "=="
  | "++"
  | "!="
  | "^"
  | "<"
  | "<<"
  | ">"
  | ">>"
  | ">>>"
  | "<="
  | ">="
  | "&"
  | "&&"
  | "|"
  | "||" => true
  | _ => false
  };
};

let prefixop = (op: string) => {
  switch (op) {
  | "!" => true
  | _ => false
  };
};

let getCommentEndLine = (comment: Parsetree.comment) => {
  let endloc =
    switch (comment) {
    | Line(cmt) => cmt.cmt_loc.loc_end
    | Block(cmt) => cmt.cmt_loc.loc_end
    | Doc(cmt) => cmt.cmt_loc.loc_end
    | Shebang(cmt) => cmt.cmt_loc.loc_end
    };

  let (_, endline, _, _) = get_raw_pos_info(endloc);
  endline;
};

let rec resugar_list_patterns =
        (
          ~patterns: list(Parsetree.pattern),
          ~parent_loc,
          ~original_source: list(string),
        ) => {
  let processed_list = resugar_pattern_list_inner(patterns, parent_loc);
  let items =
    List.map(
      i =>
        switch (i) {
        | RegularPattern(e) =>
          print_pattern(~pat=e, ~parent_loc, ~original_source)
        | SpreadPattern(e) =>
          Doc.concat([
            Doc.text("..."),
            print_pattern(~pat=e, ~parent_loc, ~original_source),
          ])
        },
      processed_list,
    );
  Doc.group(
    Doc.concat([
      Doc.lbracket,
      Doc.join(Doc.concat([Doc.comma, Doc.line]), items),
      Doc.rbracket,
    ]),
  );
}

and resugar_pattern_list_inner =
    (patterns: list(Parsetree.pattern), parent_loc) => {
  let arg1 = List.nth(patterns, 0);
  let arg2 = List.nth(patterns, 1);

  switch (arg2.ppat_desc) {
  | PPatConstruct(innerfunc, innerpatterns) =>
    let func =
      switch (innerfunc.txt) {
      | IdentName(name) => name
      | _ => ""
      };

    if (func == "[]") {
      [RegularPattern(arg1)];
    } else {
      [RegularPattern(arg1), SpreadPattern(arg2)];
    };
  | _ => [RegularPattern(arg1), SpreadPattern(arg2)]
  };
}

and is_empty_array = (expr: Parsetree.expression) => {
  switch (expr.pexp_desc) {
  | PExpId(loc) =>
    let loc_txt =
      switch (loc.txt) {
      | IdentName(name) => name
      | _ => ""
      };

    if (loc_txt == "[]") {
      true;
    } else {
      false;
    };
  | _ => false
  };
}

and resugar_list =
    (
      ~expressions: list(Parsetree.expression),
      ~parent_loc: Grain_parsing.Location.t,
      ~original_source: list(string),
    ) => {
  let processed_list = resugar_list_inner(expressions, parent_loc);

  let items =
    List.map(
      i =>
        switch (i) {
        | Regular(e) =>
          Doc.group(
            print_expression(
              ~expr=e,
              ~parentIsArrow=false,
              ~endChar=None,
              ~original_source,
              ~parent_loc,
            ),
          )
        | Spread(e) =>
          Doc.group(
            Doc.concat([
              Doc.text("..."),
              print_expression(
                ~expr=e,
                ~parentIsArrow=false,
                ~endChar=None,
                ~original_source,
                ~parent_loc,
              ),
            ]),
          )
        },
      processed_list,
    );

  Doc.group(
    Doc.indent(
      Doc.concat([
        Doc.lbracket,
        Doc.concat([
          Doc.softLine,
          Doc.join(Doc.concat([Doc.comma, Doc.line]), items),
        ]),
        Doc.softLine,
        Doc.rbracket,
      ]),
    ),
  );
}

and resugar_list_inner =
    (expressions: list(Parsetree.expression), parent_loc) => {
  let arg1 = List.nth(expressions, 0);
  let arg2 = List.nth(expressions, 1);

  switch (arg2.pexp_desc) {
  | PExpApp(innerfunc, innerexpressions) =>
    let funcName = getFunctionName(innerfunc);

    if (funcName == "[...]") {
      let inner = resugar_list_inner(innerexpressions, parent_loc);
      List.append([Regular(arg1)], inner);
    } else {
      [Regular(arg1), Spread(arg2)];
    };
  | _ =>
    if (is_empty_array(arg2)) {
      [Regular(arg1)];
    } else {
      [Regular(arg1), Spread(arg2)];
    }
  };
}

and print_record_pattern =
    (
      ~patternlocs:
         list(
           (
             Grain_parsing__Location.loc(Grain_parsing__Identifier.t),
             Grain_parsing__Parsetree.pattern,
           ),
         ),
      ~closedflag: Grain_parsing__Asttypes.closed_flag,
      ~parent_loc: Grain_parsing__Location.t,
      ~original_source: list(string),
    ) => {
  let close =
    switch (closedflag) {
    | Open => Doc.concat([Doc.text(","), Doc.space, Doc.text("_")])
    | Closed => Doc.nil
    };
  Doc.concat([
    Doc.lbrace,
    Doc.space,
    Doc.join(
      Doc.concat([Doc.comma, Doc.space]),
      List.map(
        (
          patternloc: (
            Grain_parsing__Location.loc(Grain_parsing__Identifier.t),
            Grain_parsing__Parsetree.pattern,
          ),
        ) => {
          let (loc, pat) = patternloc;

          let printed_ident: Doc.t = print_ident(loc.txt);
          let printed_pat =
            print_pattern(~pat, ~parent_loc, ~original_source);

          let pun =
            switch (printed_ident) {
            | Text(i) =>
              switch ((printed_pat: Doc.t)) {
              | Text(e) => i == e
              | _ => false
              }
            | _ => false
            };
          if (pun) {
            printed_ident;
          } else {
            Doc.concat([printed_ident, Doc.text(": "), printed_pat]);
          };
        },
        patternlocs,
      ),
    ),
    close,
    Doc.space,
    Doc.rbrace,
  ]);
}

and print_pattern =
    (
      ~pat: Parsetree.pattern,
      ~parent_loc: Grain_parsing__Location.t,
      ~original_source: list(string),
    ) => {
  let (leadingComments, trailingComments) =
    Walktree.partitionComments(pat.ppat_loc, Some(parent_loc));
  Walktree.removeUsedComments(leadingComments, trailingComments);

  let exprLine = getEndLocLine(pat.ppat_loc);

  let leadingCommentDocs =
    print_multi_comments_no_space(leadingComments, exprLine);
  let trailingCommentDocs = print_multi_comments(trailingComments, exprLine);

  let printed_pattern: (Doc.t, bool) =
    switch (pat.ppat_desc) {
    | PPatAny => (Doc.text("_"), false)
    | PPatConstant(c) => (print_constant(c), false)
    | PPatVar({txt, _}) =>
      if (infixop(txt) || prefixop(txt)) {
        (Doc.concat([Doc.lparen, Doc.text(txt), Doc.rparen]), false);
      } else {
        (Doc.text(txt), false);
      }
    | PPatTuple(patterns) => (
        Doc.join(
          Doc.concat([Doc.comma, Doc.line]),
          List.map(
            p =>
              Doc.group(
                print_pattern(
                  ~pat=p,
                  ~parent_loc=pat.ppat_loc,
                  ~original_source,
                ),
              ),
            patterns,
          ),
        ),
        true,
      )
    | PPatArray(patterns) => (
        Doc.group(
          Doc.concat([
            Doc.lbracket,
            Doc.text(">"),
            Doc.space,
            Doc.join(
              Doc.concat([Doc.comma, Doc.space]),
              List.map(
                p => print_pattern(~pat=p, ~parent_loc, ~original_source),
                patterns,
              ),
            ),
            Doc.rbracket,
          ]),
        ),
        false,
      )
    | PPatRecord(patternlocs, closedflag) => (
        print_record_pattern(
          ~patternlocs,
          ~closedflag,
          ~parent_loc,
          ~original_source,
        ),
        false,
      )
    | PPatConstraint(pattern, parsed_type) => (
        Doc.concat([
          print_pattern(
            ~pat=pattern,
            ~parent_loc=pat.ppat_loc,
            ~original_source,
          ),
          Doc.concat([Doc.text(":"), Doc.space]),
          print_type(parsed_type),
        ]),
        false,
      )
    | PPatConstruct(location, patterns) =>
      let func =
        switch (location.txt) {
        | IdentName(name) => name
        | _ => ""
        };
      if (func == "[...]") {
        (
          resugar_list_patterns(~patterns, ~parent_loc, ~original_source),
          false,
        );
      } else {
        (
          Doc.concat([
            print_ident(location.txt),
            if (List.length(patterns) > 0) {
              addParens(
                Doc.join(
                  Doc.comma,
                  List.map(
                    pat => print_pattern(~pat, ~parent_loc, ~original_source),
                    patterns,
                  ),
                ),
              );
            } else {
              Doc.nil;
            },
          ]),
          false,
        );
      };

    | PPatOr(pattern1, pattern2) => (
        Doc.text("/* Formatter PPatOr error */"),
        false,
      )
    | PPatAlias(pattern, loc) => (
        Doc.text("/* Formatter PPatAlias error */"),
        false,
      )
    };

  let (pattern, parens) = printed_pattern;

  let withLeading =
    if (leadingCommentDocs == Doc.nil) {
      [pattern];
    } else {
      [leadingCommentDocs, Doc.space, pattern];
    };
  let withTrailing =
    if (trailingCommentDocs == Doc.nil) {
      withLeading;
    } else {
      List.append(withLeading, [trailingCommentDocs]);
    };

  let cleanPattern =
    if (List.length(withTrailing) == 1) {
      List.hd(withTrailing);
    } else {
      Doc.concat(withTrailing);
    };

  if (parens) {
    Doc.concat([Doc.lparen, cleanPattern, Doc.rparen]);
  } else {
    cleanPattern;
  };
}
and print_constant = (c: Parsetree.constant) => {
  let print_c =
    switch (c) {
    | PConstNumber(PConstNumberInt(i)) => Printf.sprintf("%s", i)
    | PConstNumber(PConstNumberFloat(f)) => Printf.sprintf("%s", f)
    | PConstNumber(PConstNumberRational(n, d)) =>
      Printf.sprintf("%s/%s", n, d)
    | PConstBytes(b) => Printf.sprintf("%s", b)
    | PConstChar(c) => Printf.sprintf("'%s'", String.escaped(c))
    | PConstFloat64(f) => Printf.sprintf("%sd", f)
    | PConstFloat32(f) => Printf.sprintf("%sf", f)
    | PConstInt32(i) => Printf.sprintf("%sl", i)
    | PConstInt64(i) => Printf.sprintf("%sL", i)
    | PConstWasmI32(i) => Printf.sprintf("%sn", i)
    | PConstWasmI64(i) => Printf.sprintf("%sN", i)
    | PConstWasmF32(f) => Printf.sprintf("%sw", f)
    | PConstWasmF64(f) => Printf.sprintf("%sW", f)
    | PConstBool(true) => "true"
    | PConstBool(false) => "false"
    | PConstVoid => "void"
    | PConstString(s) => Printf.sprintf("\"%s\"", String.escaped(s))
    };
  Doc.text(print_c);
}
and print_ident = (ident: Identifier.t) => {
  switch (ident) {
  | IdentName(name) =>
    if (infixop(name) || prefixop(name)) {
      Doc.concat([Doc.lparen, Doc.text(name), Doc.rparen]);
    } else {
      Doc.text(name);
    }
  | IdentExternal(externalIdent, second) =>
    Doc.concat([
      print_ident(externalIdent),
      Doc.text("."),
      Doc.text(second),
    ])
  };
}

and print_imported_ident = (ident: Identifier.t) => {
  switch (ident) {
  | IdentName(name) =>
    if (infixop(name) || prefixop(name)) {
      Doc.concat([Doc.lparen, Doc.text(name), Doc.rparen]);
    } else {
      Doc.text(name);
    }

  | IdentExternal(externalIdent, second) =>
    Doc.concat([
      print_ident(externalIdent),
      Doc.text("."),
      Doc.text(second),
    ])
  };
}
and print_record =
    (
      ~fields:
         list(
           (
             Grain_parsing__Location.loc(Grain_parsing__Identifier.t),
             Grain_parsing__Parsetree.expression,
           ),
         ),
      ~parent_loc: Grain_parsing__Location.t,
      ~original_source: list(string),
    ) =>
  Doc.concat([
    Doc.lbrace,
    Doc.indent(
      Doc.concat([
        Doc.line,
        Doc.join(
          Doc.concat([Doc.comma, Doc.line]),
          List.map(
            (
              field: (
                Grain_parsing__Location.loc(Grain_parsing__Identifier.t),
                Grain_parsing__Parsetree.expression,
              ),
            ) => {
              let (locidentifier, expr) = field;
              let ident = locidentifier.txt;

              let printed_ident = print_ident(ident);
              let printed_expr =
                print_expression(
                  ~expr,
                  ~parentIsArrow=false,
                  ~endChar=None,
                  ~original_source,
                  ~parent_loc,
                );
              let punned_expr = check_for_pun(expr);

              let pun =
                switch (printed_ident) {
                | Text(i) =>
                  switch ((punned_expr: Doc.t)) {
                  | Text(e) => i == e
                  | _ => false
                  }
                | _ => false
                };

              if (!pun) {
                Doc.group(
                  Doc.concat([printed_ident, Doc.text(": "), printed_expr]),
                );
              } else {
                Doc.group(printed_ident);
              };
            },
            fields,
          ),
        ),
        if (List.length(fields) == 1) {
          Doc.comma; // always append a comma as single argument record look like block {data:val}
        } else {
          Doc.ifBreaks(Doc.text(","), Doc.nil);
        },
      ]),
    ),
    Doc.line,
    Doc.rbrace,
  ])

and print_type = (p: Grain_parsing__Parsetree.parsed_type) => {
  switch (p.ptyp_desc) {
  | PTyAny => Doc.text("AnyType")
  | PTyVar(name) => Doc.text(name)
  | PTyArrow(types, parsed_type) =>
    Doc.concat([
      Doc.group(
        if (List.length(types) == 1) {
          print_type(List.hd(types));
        } else {
          Doc.concat([
            Doc.lparen,
            Doc.join(
              Doc.concat([Doc.comma, Doc.space]),
              List.map(t => print_type(t), types),
            ),
            Doc.rparen,
          ]);
        },
      ),
      Doc.text(" -> "),
      print_type(parsed_type),
    ])

  | PTyTuple(parsed_types) =>
    addParens(
      Doc.join(Doc.comma, List.map(t => print_type(t), parsed_types)),
    )

  | PTyConstr(locidentifier, parsedtypes) =>
    let ident = locidentifier.txt;
    if (List.length(parsedtypes) == 0) {
      print_ident(ident);
    } else {
      Doc.concat([
        print_ident(ident),
        Doc.text("<"),
        Doc.join(
          Doc.concat([Doc.comma, Doc.space]),
          List.map(typ => {print_type(typ)}, parsedtypes),
        ),
        Doc.text(">"),
      ]);
    };
  | PTyPoly(locationstrings, parsed_type) =>
    Doc.text("/* Formatter error PTyPoly*/")
  };
}
and print_application =
    (
      ~func: Parsetree.expression,
      ~expressions: list(Parsetree.expression),
      ~parent_loc: Location.t,
      ~original_source: list(string),
    ) => {
  let functionName = getFunctionName(func);

  if (prefixop(functionName)) {
    if (List.length(expressions) == 1) {
      let first = List.hd(expressions);

      switch (first.pexp_desc) {
      | PExpApp(fn, _) =>
        Doc.concat([
          Doc.text(functionName),
          Doc.lparen,
          print_expression(
            ~expr=first,
            ~parentIsArrow=false,
            ~endChar=None,
            ~original_source,
            ~parent_loc,
          ),
          Doc.rparen,
        ])

      | _ =>
        Doc.concat([
          Doc.text(functionName),
          print_expression(
            ~expr=first,
            ~parentIsArrow=false,
            ~endChar=None,
            ~original_source,
            ~parent_loc,
          ),
        ])
      };
    } else {
      Doc.text(
        functionName
        ++ "/* formatter error prefix should have 1 arg exactly */",
      );
    };
  } else if (infixop(functionName)) {
    let first = List.hd(expressions);
    let second = List.hd(List.tl(expressions)); // assumes an infix only has two expressions
    let firstBrackets =
      switch (first.pexp_desc) {
      | PExpIf(_) =>
        Doc.concat([
          Doc.lparen,
          print_expression(
            ~expr=first,
            ~parentIsArrow=false,
            ~endChar=None,
            ~original_source,
            ~parent_loc,
          ),
          Doc.rparen,
        ])
      | PExpApp(fn, _) =>
        let leftfn = getFunctionName(fn);
        if (infixop(leftfn)) {
          Doc.concat([
            Doc.lparen,
            print_expression(
              ~expr=first,
              ~parentIsArrow=false,
              ~endChar=None,
              ~original_source,
              ~parent_loc,
            ),
            Doc.rparen,
          ]);
        } else {
          Doc.concat([
            Doc.lparen,
            print_expression(
              ~expr=first,
              ~parentIsArrow=false,
              ~endChar=None,
              ~original_source,
              ~parent_loc,
            ),
            Doc.rparen,
          ]);
        };
      | _ =>
        print_expression(
          ~expr=first,
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc,
        )
      };

    let secondBrackets =
      switch (second.pexp_desc) {
      | PExpIf(_)
      | PExpApp(_) =>
        Doc.concat([
          Doc.lparen,
          print_expression(
            ~expr=second,
            ~parentIsArrow=false,
            ~endChar=None,
            ~original_source,
            ~parent_loc,
          ),
          Doc.rparen,
        ])
      | _ =>
        print_expression(
          ~expr=second,
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc,
        )
      };
    Doc.concat([
      firstBrackets,
      Doc.space,
      Doc.text(functionName),
      Doc.space,
      secondBrackets,
    ]);
  } else {
    let funcName = getFunctionName(func);

    if (funcName == "[...]") {
      resugar_list(~expressions, ~parent_loc, ~original_source);
    } else if (funcName == "throw"
               || funcName == "assert"
               || funcName == "fail") {
      Doc.concat([
        print_expression(
          ~expr=func,
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc,
        ),
        Doc.space,
        print_expression(
          ~expr=List.hd(expressions),
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc,
        ),
      ]);
    } else if (funcName == "!") {
      Doc.concat([
        print_expression(
          ~expr=func,
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc,
        ),
        print_expression(
          ~expr=List.hd(expressions),
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc,
        ),
      ]);
    } else {
      Doc.group(
        Doc.concat([
          print_expression(
            ~expr=func,
            ~parentIsArrow=false,
            ~endChar=None,
            ~original_source,
            ~parent_loc=func.pexp_loc,
          ),
          Doc.lparen,
          Doc.indent(
            Doc.concat([
              Doc.softLine,
              Doc.join(
                Doc.concat([Doc.text(","), Doc.line]),
                List.map(
                  e =>
                    print_expression(
                      ~expr=e,
                      ~parentIsArrow=false,
                      ~endChar=None,
                      ~original_source,
                      ~parent_loc,
                    ),
                  expressions,
                ),
              ),
              Doc.ifBreaks(Doc.comma, Doc.nil),
            ]),
          ),
          Doc.softLine,
          Doc.rparen,
        ]),
      );
    };
  };
}

and getFunctionName = (expr: Parsetree.expression) => {
  switch (expr.pexp_desc) {
  | PExpConstant(x) =>
    print_endline("PExpConstant");
    Doc.toString(~width=1000, print_constant(x));
  | PExpId({txt: id}) =>
    switch (id) {
    | IdentName(name) => name
    | _ => ""
    }
  | _ => ""
  };
}

and check_for_pun = (expr: Parsetree.expression) =>
  switch (expr.pexp_desc) {
  | PExpId({txt: id}) => print_ident(id)
  | _ => Doc.nil
  }

and print_expression =
    (
      ~expr: Parsetree.expression,
      ~parentIsArrow: bool,
      ~endChar: option(Doc.t),
      ~original_source: list(string),
      ~parent_loc: Grain_parsing__Location.t,
    ) => {
  let (leadingComments, trailingComments) =
    Walktree.partitionComments(expr.pexp_loc, Some(parent_loc));

  Walktree.removeUsedComments(leadingComments, trailingComments);

  let exprLine = getEndLocLine(expr.pexp_loc);

  let leadingCommentDocs =
    if (List.length(leadingComments) > 0) {
      Doc.concat([
        print_multi_comments_no_space(leadingComments, exprLine),
        Doc.space,
      ]);
    } else {
      Doc.nil;
    };

  let trailingCommentDocs =
    if (List.length(trailingComments) > 0) {
      Doc.concat([print_multi_comments(trailingComments, exprLine)]);
    } else {
      Doc.nil;
    };

  let expression_doc =
    switch (expr.pexp_desc) {
    | PExpConstant(x) => print_constant(x)
    | PExpId({txt: id}) => print_ident(id)
    | PExpLet(rec_flag, mut_flag, vbs) =>
      value_bind_print(
        Asttypes.Nonexported,
        rec_flag,
        mut_flag,
        vbs,
        parent_loc,
        original_source,
      )
    | PExpTuple(expressions) =>
      addParens(
        Doc.join(
          Doc.concat([Doc.comma, Doc.space]),
          List.map(
            e =>
              print_expression(
                ~expr=e,
                ~parentIsArrow=false,
                ~endChar=None,
                ~original_source,
                ~parent_loc,
              ),
            expressions,
          ),
        ),
      )

    | PExpArray(expressions) =>
      Doc.group(
        if (List.length(expressions) == 0) {
          Doc.text("[>]");
        } else {
          Doc.concat([
            Doc.lbracket,
            Doc.text("> "),
            Doc.join(
              Doc.concat([Doc.comma, Doc.space]),
              List.map(
                e =>
                  print_expression(
                    ~expr=e,
                    ~parentIsArrow=false,
                    ~endChar=None,
                    ~original_source,
                    ~parent_loc,
                  ),
                expressions,
              ),
            ),
            Doc.space,
            Doc.rbracket,
          ]);
        },
      )
    | PExpArrayGet(expression1, expression2) =>
      Doc.concat([
        print_expression(
          ~expr=expression1,
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc,
        ),
        Doc.lbracket,
        print_expression(
          ~expr=expression2,
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc,
        ),
        Doc.rbracket,
      ])
    | PExpArraySet(expression1, expression2, expression3) =>
      Doc.group(
        Doc.concat([
          print_expression(
            ~expr=expression1,
            ~parentIsArrow=false,
            ~endChar=None,
            ~original_source,
            ~parent_loc,
          ),
          Doc.lbracket,
          print_expression(
            ~expr=expression2,
            ~parentIsArrow=false,
            ~endChar=None,
            ~original_source,
            ~parent_loc,
          ),
          Doc.rbracket,
          Doc.text(" ="),
          Doc.indent(
            Doc.concat([
              Doc.line,
              print_expression(
                ~expr=expression3,
                ~parentIsArrow=false,
                ~endChar=None,
                ~original_source,
                ~parent_loc,
              ),
            ]),
          ),
        ]),
      )

    | PExpRecord(record) =>
      print_record(~fields=record, ~parent_loc, ~original_source)
    | PExpRecordGet(expression, {txt, _}) =>
      Doc.concat([
        print_expression(
          ~expr=expression,
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc,
        ),
        Doc.dot,
        print_ident(txt),
      ])
    | PExpRecordSet(expression, {txt, _}, expression2) =>
      Doc.concat([
        print_expression(
          ~expr=expression,
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc,
        ),
        Doc.dot,
        print_ident(txt),
        Doc.text(" = "),
        print_expression(
          ~expr=expression2,
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc,
        ),
      ])
    | PExpMatch(expression, match_branches) =>
      let arg =
        Doc.concat([
          Doc.lparen,
          Doc.group(
            print_expression(
              ~expr=expression,
              ~parentIsArrow=false,
              ~endChar=None,
              ~original_source,
              ~parent_loc,
            ),
          ),
          Doc.rparen,
        ]);

      Doc.breakableGroup(
        ~forceBreak=false,
        Doc.concat([
          Doc.concat([Doc.text("match "), arg, Doc.space]),
          Doc.text("{"),
          Doc.indent(
            Doc.concat([
              Doc.hardLine,
              Doc.join(
                Doc.line, // need to inject comma before comments
                List.map(
                  (branch: Parsetree.match_branch) => {
                    let (leadingComments, trailingComments) =
                      Walktree.partitionComments(
                        branch.pmb_loc,
                        Some(expr.pexp_loc),
                      );

                    let this_line = getEndLocLine(branch.pmb_loc);

                    let (thisLineComments, belowLineComments) =
                      split_comments(trailingComments, this_line);

                    Walktree.removeUsedComments(
                      leadingComments,
                      trailingComments,
                    );

                    let (stmtLeadingCommentDocs1, prevLine) =
                      print_leading_comments(leadingComments, this_line);
                    let stmtLeadingCommentDocs =
                      if (List.length(leadingComments) > 0) {
                        stmtLeadingCommentDocs1;
                      } else {
                        Doc.nil;
                      };
                    let trailingLineCommentDocs =
                      print_multi_comments(thisLineComments, this_line);
                    let belowLineCommentDocs =
                      print_multi_comments(belowLineComments, this_line);

                    //
                    Doc.concat([
                      Doc.group(
                        Doc.concat([
                          stmtLeadingCommentDocs,
                          Doc.concat([
                            Doc.group(
                              print_pattern(
                                ~pat=branch.pmb_pat,
                                ~parent_loc,
                                ~original_source,
                              ),
                            ),
                            switch (branch.pmb_guard) {
                            | None => Doc.nil
                            | Some(guard) =>
                              Doc.concat([
                                Doc.text(" when "),
                                print_expression(
                                  ~expr=guard,
                                  ~parentIsArrow=false,
                                  ~endChar=None,
                                  ~original_source,
                                  ~parent_loc,
                                ),
                              ])
                            },
                            Doc.text(" =>"),
                          ]),
                          Doc.group(
                            switch (branch.pmb_body.pexp_desc) {
                            | PExpBlock(expressions) =>
                              Doc.concat([
                                Doc.space,
                                print_expression(
                                  ~expr=branch.pmb_body,
                                  ~parentIsArrow=true,
                                  ~endChar=None,
                                  ~original_source,
                                  ~parent_loc,
                                ),
                              ])
                            | _ =>
                              Doc.indent(
                                Doc.concat([
                                  Doc.line,
                                  print_expression(
                                    ~expr=branch.pmb_body,
                                    ~parentIsArrow=true,
                                    ~endChar=None,
                                    ~original_source,
                                    ~parent_loc,
                                  ),
                                ]),
                              )
                            },
                          ),
                          Doc.comma,
                          trailingLineCommentDocs,
                        ]),
                      ),
                      if (List.length(belowLineComments) > 0) {
                        Doc.concat([belowLineCommentDocs]);
                      } else {
                        Doc.nil;
                      },
                    ]);
                  },
                  match_branches,
                ),
              ),
              // keeping this as I think we can start to not add commas again
              //  Doc.text(","),
              //Doc.ifBreaks(Doc.text(","), Doc.nil),
            ]),
          ),
          Doc.line,
          Doc.text("}"),
        ]),
      );

    | PExpPrim1(prim1, expression) =>
      Doc.text("/* PExpPrim1 not handled by formatter */")
    | PExpPrim2(prim2, expression, expression1) =>
      Doc.text(" /*PExpPrim2 not handled by formatter */")
    | PExpPrimN(primn, expressions) =>
      Doc.text("/*b PExpPrimN not handled by formatter */")
    | PExpIf(condition, trueExpr, falseExpr) =>
      let (leadingConditionComments, _trailingComments) =
        Walktree.partitionComments(condition.pexp_loc, Some(parent_loc));
      Walktree.removeUsedComments(leadingConditionComments, []);

      let exprLine = getEndLocLine(condition.pexp_loc);

      let leadingConditionCommentDocs =
        if (List.length(leadingConditionComments) > 0) {
          Doc.concat([
            print_multi_comments_no_space(leadingConditionComments, exprLine),
            Doc.space,
          ]);
        } else {
          Doc.nil;
        };

      let trueFalseSpace = Doc.space;
      // keep this - we need this if we don't force single lines into block expressions
      // switch (trueExpr.pexp_desc) {
      // | PExpBlock(expressions) => Doc.space
      // | _ => Doc.line
      // };

      let trueClause =
        switch (trueExpr.pexp_desc) {
        | PExpBlock(expressions) =>
          print_expression(
            ~expr=trueExpr,
            ~parentIsArrow=false,
            ~endChar=None,
            ~original_source,
            ~parent_loc,
          )

        | _ =>
          Doc.concat([
            Doc.lbrace,
            Doc.indent(
              Doc.concat([
                Doc.hardLine,
                print_expression(
                  ~expr=trueExpr,
                  ~parentIsArrow=false,
                  ~endChar=None,
                  ~original_source,
                  ~parent_loc,
                ),
              ]),
            ),
            Doc.hardLine,
            Doc.rbrace,
          ])
        };

      let falseClause =
        switch (falseExpr.pexp_desc) {
        | PExpBlock(expressions) =>
          if (List.length(expressions) > 0) {
            Doc.concat([
              trueFalseSpace,
              Doc.text("else "),
              print_expression(
                ~expr=falseExpr,
                ~parentIsArrow=false,
                ~endChar=None,
                ~original_source,
                ~parent_loc,
              ),
            ]);
          } else {
            Doc.nil;
          }
        | PExpIf(_condition, _trueExpr, _falseExpr) =>
          Doc.concat([
            trueFalseSpace,
            Doc.text("else "),
            print_expression(
              ~expr=falseExpr,
              ~parentIsArrow=false,
              ~endChar=None,
              ~original_source,
              ~parent_loc,
            ),
          ])
        | _ =>
          Doc.concat([
            trueFalseSpace,
            Doc.text("else"),
            Doc.group(
              Doc.concat([
                Doc.space,
                Doc.lbrace,
                Doc.indent(
                  Doc.concat([
                    Doc.hardLine,
                    print_expression(
                      ~expr=falseExpr,
                      ~parentIsArrow=false,
                      ~endChar=None,
                      ~original_source,
                      ~parent_loc,
                    ),
                  ]),
                ),
                Doc.hardLine,
                Doc.rbrace,
              ]),
            ),
          ])
        };

      if (parentIsArrow) {
        Doc.group(
          Doc.concat([
            Doc.text("if "),
            Doc.group(
              Doc.concat([
                Doc.lparen,
                leadingConditionCommentDocs,
                print_expression(
                  ~expr=condition,
                  ~parentIsArrow=false,
                  ~endChar=None,
                  ~original_source,
                  ~parent_loc,
                ),
                Doc.rparen,
                Doc.space,
              ]),
            ),
            trueClause,
            falseClause,
          ]),
        );
      } else {
        Doc.concat([
          Doc.group(
            Doc.concat([
              Doc.text("if "),
              Doc.lparen,
              leadingConditionCommentDocs,
              print_expression(
                ~expr=condition,
                ~parentIsArrow=false,
                ~endChar=None,
                ~original_source,
                ~parent_loc,
              ),
              Doc.rparen,
              Doc.space,
            ]),
          ),
          trueClause,
          falseClause,
        ]);
      };
    | PExpWhile(expression, expression1) =>
      Doc.concat([
        Doc.text("while "),
        Doc.group(
          Doc.concat([
            Doc.lparen,
            Doc.space,
            print_expression(
              ~expr=expression,
              ~parentIsArrow=false,
              ~endChar=None,
              ~original_source,
              ~parent_loc,
            ),
            Doc.space,
            Doc.rparen,
          ]),
        ),
        Doc.space,
        Doc.group(
          print_expression(
            ~expr=expression1,
            ~parentIsArrow=false,
            ~endChar=None,
            ~original_source,
            ~parent_loc,
          ),
        ),
      ])

    | PExpFor(optexpression1, optexpression2, optexpression3, expression4) =>
      Doc.concat([
        Doc.group(
          Doc.concat([
            Doc.text("for "),
            Doc.lparen,
            Doc.indent(
              Doc.concat([
                Doc.line,
                Doc.concat([
                  switch (optexpression1) {
                  | Some(expr) =>
                    print_expression(
                      ~expr,
                      ~parentIsArrow=false,
                      ~endChar=None,
                      ~original_source,
                      ~parent_loc,
                    )
                  | None => Doc.nil
                  },
                  Doc.text(";"),
                  switch (optexpression2) {
                  | Some(expr) =>
                    Doc.concat([
                      Doc.line,
                      Doc.group(
                        print_expression(
                          ~expr,
                          ~parentIsArrow=false,
                          ~endChar=None,
                          ~original_source,
                          ~parent_loc,
                        ),
                      ),
                    ])
                  | None => Doc.nil
                  },
                  Doc.text(";"),
                  switch (optexpression3) {
                  | Some(expr) =>
                    Doc.concat([
                      Doc.line,
                      Doc.group(
                        print_expression(
                          ~expr,
                          ~parentIsArrow=false,
                          ~endChar=None,
                          ~original_source,
                          ~parent_loc,
                        ),
                      ),
                    ])
                  | None => Doc.nil
                  },
                ]),
              ]),
            ),
            Doc.line,
            Doc.rparen,
          ]),
        ),
        Doc.space,
        print_expression(
          ~expr=expression4,
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc,
        ),
      ])
    | PExpContinue => Doc.group(Doc.concat([Doc.text("continue")]))
    | PExpBreak => Doc.group(Doc.concat([Doc.text("break")]))
    | PExpConstraint(expression, parsed_type) =>
      Doc.group(
        Doc.concat([
          print_expression(
            ~expr=expression,
            ~parentIsArrow=false,
            ~endChar=None,
            ~original_source,
            ~parent_loc,
          ),
          Doc.text(": "),
          print_type(parsed_type),
        ]),
      )
    | PExpLambda(patterns, expression) =>
      let args =
        if (List.length(patterns) == 0) {
          Doc.concat([Doc.lparen, Doc.rparen]);
        } else if (List.length(patterns) > 1) {
          Doc.group(
            Doc.indent(
              Doc.concat([
                Doc.softLine,
                Doc.lparen,
                Doc.indent(
                  Doc.concat([
                    Doc.softLine,
                    Doc.join(
                      Doc.concat([Doc.text(","), Doc.line]),
                      List.map(
                        p =>
                          print_pattern(
                            ~pat=p,
                            ~parent_loc,
                            ~original_source,
                          ),
                        patterns,
                      ),
                    ),
                    Doc.ifBreaks(Doc.text(","), Doc.nil),
                  ]),
                ),
                Doc.softLine,
                Doc.rparen,
              ]),
            ),
          );
        } else {
          let pat = List.hd(patterns);

          switch (pat.ppat_desc) {
          | PPatConstraint(_) =>
            Doc.concat([
              Doc.lparen,
              print_pattern(~pat, ~parent_loc, ~original_source),
              Doc.rparen,
            ])
          | PPatTuple(_) =>
            Doc.concat([
              Doc.lparen,
              print_pattern(~pat, ~parent_loc, ~original_source),
              Doc.rparen,
            ])
          | _ => print_pattern(~pat, ~parent_loc, ~original_source)
          };
        };

      let followsArrow =
        switch (expression.pexp_desc) {
        | PExpBlock(_) => [
            Doc.group(
              Doc.concat([args, Doc.space, Doc.text("=>"), Doc.space]),
            ),
            Doc.group(
              print_expression(
                ~expr=expression,
                ~parentIsArrow=false,
                ~endChar=None,
                ~original_source,
                ~parent_loc,
              ),
            ),
          ]
        | _ => [
            Doc.concat([
              args,
              Doc.space,
              Doc.text("=>"),
              Doc.indent(
                Doc.concat([
                  Doc.line,
                  Doc.group(
                    print_expression(
                      ~expr=expression,
                      ~parentIsArrow=false,
                      ~endChar=None,
                      ~original_source,
                      ~parent_loc,
                    ),
                  ),
                ]),
              ),
            ]),
          ]
        };

      Doc.concat(followsArrow);

    | PExpApp(func, expressions) =>
      print_application(~func, ~expressions, ~parent_loc, ~original_source)
    | PExpBlock(expressions) =>
      if (List.length(expressions) > 0) {
        let previousLine = ref(getLocLine(expr.pexp_loc));
        let block =
          Doc.join(
            Doc.hardLine,
            List.map(
              (e: Parsetree.expression) => {
                // get the leading comments
                let (leadingComments, _trailingComments) =
                  Walktree.partitionComments(e.pexp_loc, None);

                let disableFormatting =
                  List.exists(
                    c => isDisableFormattingComment(c),
                    leadingComments,
                  );

                Walktree.removeUsedComments(leadingComments, []);

                let (stmtLeadingCommentDocs1, prevLine) =
                  print_leading_comments(leadingComments, previousLine^);

                let stmtLeadingCommentDocs =
                  if (List.length(leadingComments) > 0) {
                    stmtLeadingCommentDocs1;
                  } else {
                    Doc.nil;
                  };

                let blankLineAbove =
                  if (getLocLine(e.pexp_loc) > prevLine + 1) {
                    Doc.hardLine;
                  } else {
                    Doc.nil;
                  };

                if (disableFormatting) {
                  let originalCode =
                    getOriginalCode(e.pexp_loc, original_source);
                  // need to remove any comments that were inside the disabled block

                  Walktree.removeCommentsInIgnoreBlock(e.pexp_loc);

                  Doc.concat([
                    stmtLeadingCommentDocs,
                    blankLineAbove,
                    Doc.group(Doc.text(originalCode)),
                  ]);
                } else {
                  let printed_expression =
                    print_expression(
                      ~expr=e,
                      ~parentIsArrow=false,
                      ~endChar=None,
                      ~original_source,
                      ~parent_loc,
                    );

                  previousLine := getEndLocLine(e.pexp_loc);

                  Doc.group(
                    Doc.concat([
                      stmtLeadingCommentDocs,
                      blankLineAbove,
                      Doc.group(printed_expression),
                    ]),
                  );
                };
              },
              expressions,
            ),
          );

        Doc.breakableGroup(
          ~forceBreak=true,
          Doc.concat([
            Doc.lbrace,
            Doc.indent(Doc.concat([Doc.line, block])),
            Doc.line,
            Doc.rbrace,
          ]),
        );
      } else {
        let (_leadingComments, trailingComments) =
          Walktree.partitionComments(expr.pexp_loc, Some(parent_loc));

        Walktree.removeUsedComments([], trailingComments);

        let exprLine = getEndLocLine(expr.pexp_loc) + 1;

        let blockCommentDocs =
          if (List.length(trailingComments) > 0) {
            Doc.concat([
              print_multi_comments_no_space(trailingComments, exprLine),
              Doc.hardLine,
            ]);
          } else {
            Doc.nil;
          };
        Doc.breakableGroup(
          ~forceBreak=true,
          Doc.concat([
            Doc.lbrace,
            Doc.indent(Doc.concat([Doc.line, blockCommentDocs])),
            Doc.line,
            Doc.rbrace,
          ]),
        );
      }

    | PExpBoxAssign(expression, expression1) =>
      Doc.concat([
        print_expression(
          ~expr=expression,
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc,
        ),
        Doc.text(" := "),
        print_expression(
          ~expr=expression1,
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc,
        ),
      ])
    | PExpAssign(expression, expression1) =>
      switch (expression1.pexp_desc) {
      | PExpApp(func, expressions) =>
        let functionName = getFunctionName(func);

        let trimmedOperator = String.trim(functionName);

        let left =
          print_expression(
            ~expr=expression,
            ~parentIsArrow=false,
            ~endChar=None,
            ~parent_loc,
            ~original_source,
          );

        let first =
          print_expression(
            ~expr=List.hd(expressions),
            ~parentIsArrow=false,
            ~endChar=None,
            ~parent_loc,
            ~original_source,
          );

        if (left == first) {
          // +=, -=, *=, /=, and %=
          switch (trimmedOperator) {
          | "+"
          | "-"
          | "*"
          | "/"
          | "%" =>
            let sugaredOp = Doc.text(" " ++ trimmedOperator ++ "= ");
            Doc.concat([
              print_expression(
                ~expr=expression,
                ~parentIsArrow=false,
                ~endChar=None,
                ~original_source,
                ~parent_loc,
              ),
              sugaredOp,
              if (List.length(expressions) == 1) {
                print_expression(
                  ~expr=List.hd(expressions),
                  ~parentIsArrow=false,
                  ~endChar=None,
                  ~original_source,
                  ~parent_loc,
                );
              } else {
                print_expression(
                  ~expr=List.hd(List.tl(expressions)),
                  ~parentIsArrow=false,
                  ~endChar=None,
                  ~original_source,
                  ~parent_loc,
                );
              },
            ]);
          | _ =>
            Doc.concat([
              print_expression(
                ~expr=expression,
                ~parentIsArrow=false,
                ~endChar=None,
                ~original_source,
                ~parent_loc,
              ),
              Doc.text(" = "),
              print_expression(
                ~expr=expression1,
                ~parentIsArrow=false,
                ~endChar=None,
                ~original_source,
                ~parent_loc,
              ),
            ])
          };
        } else {
          Doc.concat([
            print_expression(
              ~expr=expression,
              ~parentIsArrow=false,
              ~endChar=None,
              ~original_source,
              ~parent_loc,
            ),
            Doc.text(" = "),
            print_expression(
              ~expr=expression1,
              ~parentIsArrow=false,
              ~endChar=None,
              ~original_source,
              ~parent_loc,
            ),
          ]);
        };

      | _ =>
        Doc.concat([
          print_expression(
            ~expr=expression,
            ~parentIsArrow=false,
            ~endChar=None,
            ~original_source,
            ~parent_loc,
          ),
          Doc.text(" = "),
          print_expression(
            ~expr=expression1,
            ~parentIsArrow=false,
            ~endChar=None,
            ~original_source,
            ~parent_loc,
          ),
        ])
      }

    | /** Used for modules without body expressions */ PExpNull => Doc.nil
    };

  let endingDoc =
    switch (endChar) {
    | None => Doc.nil
    | Some(d) => Doc.concat([d])
    };

  if (trailingCommentDocs == Doc.nil) {
    Doc.concat([leadingCommentDocs, expression_doc, endingDoc]);
  } else {
    Doc.concat([
      leadingCommentDocs,
      expression_doc,
      endingDoc,
      trailingCommentDocs,
    ]);
  };
}
and value_bind_print =
    (
      export_flag: Asttypes.export_flag,
      rec_flag,
      mut_flag,
      vbs: list(Parsetree.value_binding),
      parent_loc: Grain_parsing__Location.t,
      original_source: list(string),
    ) => {
  let exported =
    switch (export_flag) {
    | Nonexported => Doc.nil
    | Exported => Doc.text("export ")
    };
  let recursive =
    switch (rec_flag) {
    | Nonrecursive => Doc.nil
    | Recursive => Doc.text("rec ")
    };
  let mutble =
    switch (mut_flag) {
    | Immutable => Doc.nil
    | Mutable => Doc.text("mut ")
    };

  let value_bindings =
    List.map(
      (vb: Parsetree.value_binding) => {
        let expression =
          print_expression(
            ~expr=vb.pvb_expr,
            ~parentIsArrow=false,
            ~endChar=None,
            ~original_source,
            ~parent_loc,
          );

        // fix needed here?, we may  need to work out if all arguments fit on the line or will break
        let expressionGrp =
          switch (vb.pvb_expr.pexp_desc) {
          | PExpBlock(_) => Doc.concat([Doc.space, expression])
          | _ => Doc.concat([Doc.space, expression])
          };

        Doc.concat([
          Doc.group(
            print_pattern(
              ~pat=vb.pvb_pat,
              ~parent_loc=vb.pvb_loc,
              ~original_source,
            ),
          ),
          Doc.text(" ="),
          Doc.group(expressionGrp),
        ]);
      },
      vbs,
    );

  Doc.concat([
    Doc.group(
      Doc.concat([
        exported,
        Doc.text("let "),
        recursive,
        mutble,
        Doc.join(
          Doc.concat([Doc.comma, Doc.hardLine]),
          List.map(vb => vb, value_bindings),
        ),
      ]),
    ),
  ]);
};

let rec print_data = (data: Grain_parsing__Parsetree.data_declaration) => {
  let nameloc = data.pdata_name;
  switch (data.pdata_kind) {
  | PDataVariant(constr_declarations) =>
    Doc.group(
      Doc.concat([
        Doc.text("enum"),
        Doc.space,
        Doc.text(nameloc.txt),
        if (List.length(data.pdata_params) > 0) {
          Doc.concat([
            Doc.text("<"),
            Doc.join(
              Doc.text(", "),
              List.map(t => print_type(t), data.pdata_params),
            ),
            Doc.text(">"),
            Doc.space,
          ]);
        } else {
          Doc.space;
        },
        Doc.lbrace,
        Doc.indent(
          Doc.concat([
            Doc.line,
            Doc.join(
              Doc.concat([Doc.line]),
              List.map(
                (d: Grain_parsing__Parsetree.constructor_declaration) => {
                  let (leadingComments, trailingComments) =
                    Walktree.partitionComments(
                      d.pcd_loc,
                      Some(data.pdata_loc),
                    );

                  let this_line = getEndLocLine(d.pcd_loc);

                  let (thisLineComments, belowLineComments) =
                    split_comments(trailingComments, this_line);

                  Walktree.removeUsedComments(
                    leadingComments,
                    trailingComments,
                  );

                  let (stmtLeadingCommentDocs1, prevLine) =
                    print_leading_comments(leadingComments, this_line);
                  let stmtLeadingCommentDocs =
                    if (List.length(leadingComments) > 0) {
                      stmtLeadingCommentDocs1;
                    } else {
                      Doc.nil;
                    };
                  let trailingLineCommentDocs =
                    print_multi_comments(thisLineComments, this_line);
                  let belowLineCommentDocs =
                    print_multi_comments(belowLineComments, this_line);
                  Doc.concat([
                    Doc.group(
                      Doc.concat([
                        stmtLeadingCommentDocs,
                        Doc.text(d.pcd_name.txt),
                        switch (d.pcd_args) {
                        | PConstrTuple(parsed_types) =>
                          if (List.length(parsed_types) > 0) {
                            Doc.group(
                              Doc.concat([
                                Doc.text("("),
                                Doc.join(
                                  Doc.concat([Doc.comma, Doc.line]),
                                  List.map(t => print_type(t), parsed_types),
                                ),
                                Doc.text(")"),
                              ]),
                            );
                          } else {
                            Doc.nil;
                          }
                        | PConstrSingleton => Doc.nil
                        },
                        Doc.comma,
                        trailingLineCommentDocs,
                      ]),
                    ),
                    if (List.length(belowLineComments) > 0) {
                      Doc.concat([belowLineCommentDocs]);
                    } else {
                      Doc.nil;
                    },
                  ]);
                },
                constr_declarations,
              ),
            ),
          ]),
        ),
        Doc.line,
        Doc.rbrace,
      ]),
    )

  | PDataRecord(label_declarations) =>
    Doc.group(
      Doc.concat([
        Doc.text("record"),
        Doc.space,
        Doc.text(nameloc.txt),
        if (List.length(data.pdata_params) > 0) {
          Doc.concat([
            Doc.text("<"),
            Doc.join(
              Doc.concat([Doc.text(","), Doc.space]),
              List.map(t => print_type(t), data.pdata_params),
            ),
            Doc.text(">"),
            Doc.space,
          ]);
        } else {
          Doc.space;
        },
        Doc.concat([
          Doc.lbrace,
          Doc.indent(
            Doc.concat([
              Doc.line,
              Doc.join(
                Doc.concat([Doc.line]),
                List.map(
                  (decl: Grain_parsing__Parsetree.label_declaration) => {
                    let isMutable =
                      switch (decl.pld_mutable) {
                      | Mutable => Doc.text("mut ")
                      | Immutable => Doc.nil
                      };

                    let (leadingComments, trailingComments) =
                      Walktree.partitionComments(
                        decl.pld_loc,
                        Some(data.pdata_loc),
                      );

                    let this_line = getEndLocLine(decl.pld_loc);

                    let (thisLineComments, belowLineComments) =
                      split_comments(trailingComments, this_line);

                    Walktree.removeUsedComments(
                      leadingComments,
                      trailingComments,
                    );

                    let (stmtLeadingCommentDocs1, prevLine) =
                      print_leading_comments(leadingComments, this_line);
                    let stmtLeadingCommentDocs =
                      if (List.length(leadingComments) > 0) {
                        stmtLeadingCommentDocs1;
                      } else {
                        Doc.nil;
                      };
                    let trailingLineCommentDocs =
                      print_multi_comments(thisLineComments, this_line);
                    let belowLineCommentDocs =
                      print_multi_comments(belowLineComments, this_line);

                    Doc.concat([
                      Doc.group(
                        Doc.concat([
                          stmtLeadingCommentDocs,
                          isMutable,
                          print_ident(decl.pld_name.txt),
                          Doc.text(":"),
                          Doc.space,
                          print_type(decl.pld_type),
                          Doc.comma,
                          trailingLineCommentDocs,
                        ]),
                      ),
                      if (List.length(belowLineComments) > 0) {
                        Doc.concat([belowLineCommentDocs]);
                      } else {
                        Doc.nil;
                      },
                    ]);
                  },
                  label_declarations,
                ),
              ),
            ]),
          ),
          Doc.line,
          Doc.rbrace,
        ]),
      ]),
    )
  };
};
let data_print =
    (
      datas:
        list(
          (
            Grain_parsing__Parsetree.export_flag,
            Grain_parsing__Parsetree.data_declaration,
          ),
        ),
    ) => {
  Doc.join(
    Doc.text(","),
    List.map(
      data => {
        let (expt, decl) = data;
        Doc.concat([
          switch ((expt: Asttypes.export_flag)) {
          | Nonexported => Doc.nil
          | Exported => Doc.text("export ")
          },
          print_data(decl),
        ]);
      },
      datas,
    ),
  );
};
let import_print = (imp: Parsetree.import_declaration) => {
  let vals =
    List.map(
      (v: Parsetree.import_value) => {
        switch (v) {
        | PImportModule(identloc) => print_ident(identloc.txt)
        | PImportAllExcept(identlocs) =>
          Doc.concat([
            Doc.text("*"),
            if (List.length(identlocs) > 0) {
              Doc.concat([
                Doc.text(" except "),
                addBraces(
                  Doc.join(
                    Doc.comma,
                    List.map(
                      (identloc: Location.loc(Grain_parsing__Identifier.t)) =>
                        print_ident(identloc.txt),
                      identlocs,
                    ),
                  ),
                ),
              ]);
            } else {
              Doc.nil;
            },
          ])
        | PImportValues(identlocsopts) =>
          Doc.concat([
            Doc.lbrace,
            Doc.indent(
              Doc.concat([
                Doc.line,
                if (List.length(identlocsopts) > 0) {
                  Doc.join(
                    Doc.concat([Doc.comma, Doc.line]),
                    List.map(
                      (
                        identlocopt: (
                          Grain_parsing.Parsetree.loc(
                            Grain_parsing.Identifier.t,
                          ),
                          option(
                            Grain_parsing.Parsetree.loc(
                              Grain_parsing.Identifier.t,
                            ),
                          ),
                        ),
                      ) => {
                        let (loc, optloc) = identlocopt;
                        switch (optloc) {
                        | None => print_imported_ident(loc.txt)
                        | Some(alias) =>
                          Doc.concat([
                            print_imported_ident(loc.txt),
                            Doc.text(" as "),
                            print_imported_ident(alias.txt),
                          ])
                        };
                      },
                      identlocsopts,
                    ),
                  );
                } else {
                  Doc.nil;
                },
              ]),
            ),
            Doc.line,
            Doc.rbrace,
          ])
        }
      },
      imp.pimp_val,
    );

  let path = imp.pimp_path.txt;

  Doc.group(
    Doc.concat([
      Doc.text("import "),
      Doc.join(Doc.concat([Doc.comma, Doc.space]), vals),
      Doc.text(" from "),
      Doc.doubleQuote,
      Doc.text(path),
      Doc.doubleQuote,
    ]),
  );
};

let print_export_desc = (desc: Parsetree.export_declaration_desc) => {
  let ident = desc.pex_name.txt;

  let fixedIdent =
    if (infixop(ident) || prefixop(ident)) {
      Doc.concat([Doc.lparen, Doc.text(ident), Doc.rparen]);
    } else {
      Doc.text(ident);
    };
  Doc.concat([
    fixedIdent,
    switch (desc.pex_alias) {
    | Some(alias) => Doc.concat([Doc.text(" as "), Doc.text(alias.txt)])
    | None => Doc.nil
    },
  ]);
};

let print_export_declaration = (decl: Parsetree.export_declaration) => {
  switch (decl) {
  | ExportData(data) => print_export_desc(data)
  | ExportValue(value) => print_export_desc(value)
  };
};

let print_foreign_value_description = (vd: Parsetree.value_description) => {
  let ident = vd.pval_name.txt;

  let fixedIdent =
    if (infixop(ident) || prefixop(ident)) {
      Doc.concat([Doc.lparen, Doc.text(ident), Doc.rparen]);
    } else {
      Doc.text(ident);
    };

  Doc.concat([
    fixedIdent,
    Doc.text(" : "),
    print_type(vd.pval_type),
    Doc.text(" from "),
    Doc.text("\""),
    Doc.text(vd.pval_mod.txt),
    Doc.text("\""),
  ]);
};

let print_primitive_value_description = (vd: Parsetree.value_description) => {
  let ident = vd.pval_name.txt;

  let fixedIdent =
    if (infixop(ident) || prefixop(ident)) {
      Doc.concat([Doc.lparen, Doc.text(ident), Doc.rparen]);
    } else {
      Doc.text(ident);
    };

  Doc.concat([
    fixedIdent,
    Doc.text(" : "),
    print_type(vd.pval_type),
    Doc.text(" = "),
    Doc.text("\""),
    Doc.join(Doc.text(","), List.map(p => Doc.text(p), vd.pval_prim)),
    Doc.text("\""),
  ]);
};

let toplevel_print =
    (
      ~data: Parsetree.toplevel_stmt,
      ~original_source: list(string),
      ~previousLine: int,
    ) => {
  let attributes = data.ptop_attributes;

  let (leadingComments, _trailingComments) =
    Walktree.partitionComments(data.ptop_loc, None); //

  // check to see if we have a comment to disable formatting

  let disableFormatting =
    List.exists(c => isDisableFormattingComment(c), leadingComments);

  Walktree.removeUsedComments(leadingComments, []);

  // remove all nodes before this top level statement

  Walktree.removeNodesBefore(data.ptop_loc);

  let (stmtLeadingCommentDocs, prevLine) =
    print_leading_comments(leadingComments, previousLine);

  let stmtLeadingCommentDocs =
    if (List.length(leadingComments) > 0) {
      stmtLeadingCommentDocs;
    } else {
      Doc.nil;
    };

  let blankLineAbove =
    if (getLocLine(data.ptop_loc) > prevLine + 1) {
      Doc.hardLine;
    } else {
      Doc.nil;
    };
  let attributeText =
    if (List.length(attributes) > 0) {
      Doc.concat([
        Doc.join(
          Doc.space,
          List.map(
            (a: Location.loc(string)) =>
              Doc.concat([Doc.text("@"), Doc.text(a.txt)]),
            attributes,
          ),
        ),
        Doc.hardLine,
      ]);
    } else {
      Doc.nil;
    };

  if (disableFormatting) {
    let originalCode = getOriginalCode(data.ptop_loc, original_source);
    // need to remove any comments that were inside the disabled block

    Walktree.removeCommentsInIgnoreBlock(data.ptop_loc);

    Doc.concat([
      stmtLeadingCommentDocs,
      blankLineAbove,
      attributeText,
      Doc.group(Doc.text(originalCode)),
    ]);
  } else {
    let withoutComments =
      switch (data.ptop_desc) {
      | PTopImport(import_declaration) => import_print(import_declaration)
      | PTopForeign(export_flag, value_description) =>
        let export =
          switch (export_flag) {
          | Nonexported => Doc.text("import ")
          | Exported => Doc.text("export ")
          };
        Doc.concat([
          export,
          Doc.text("foreign wasm "),
          print_foreign_value_description(value_description),
        ]);
      | PTopPrimitive(export_flag, value_description) =>
        let export =
          switch (export_flag) {
          | Nonexported => Doc.nil
          | Exported => Doc.text("export ")
          };
        Doc.concat([
          export,
          Doc.text("primitive "),
          print_primitive_value_description(value_description),
        ]);
      | PTopData(data_declarations) => data_print(data_declarations)
      | PTopLet(export_flag, rec_flag, mut_flag, value_bindings) =>
        value_bind_print(
          export_flag,
          rec_flag,
          mut_flag,
          value_bindings,
          data.ptop_loc,
          original_source,
        )
      | PTopExpr(expression) =>
        print_expression(
          ~expr=expression,
          ~parentIsArrow=false,
          ~endChar=None,
          ~original_source,
          ~parent_loc=data.ptop_loc,
        )
      | PTopException(export_flag, type_exception) =>
        let export =
          switch (export_flag) {
          | Nonexported => Doc.nil
          | Exported => Doc.text("export ")
          };
        let cstr = type_exception.ptyexn_constructor;

        let kind =
          switch (cstr.pext_kind) {
          | PExtDecl(sargs) =>
            switch (sargs) {
            | PConstrSingleton => Doc.nil
            | PConstrTuple(parsed_types) =>
              if (List.length(parsed_types) > 0) {
                Doc.concat([
                  Doc.text("("),
                  Doc.join(
                    Doc.comma,
                    List.map(t => print_type(t), parsed_types),
                  ),
                  Doc.text(")"),
                ]);
              } else {
                Doc.nil;
              }
            }

          | PExtRebind(lid) => print_ident(lid.txt)
          };

        Doc.concat([
          export,
          Doc.text("exception "),
          Doc.text(cstr.pext_name.txt),
          kind,
        ]);
      | PTopExport(export_declarations) =>
        Doc.concat([
          Doc.text("export "),
          Doc.join(
            Doc.concat([Doc.comma, Doc.line]),
            List.map(e => print_export_declaration(e), export_declarations),
          ),
        ])

      | PTopExportAll(export_excepts) =>
        Doc.concat([
          Doc.text("export * "),
          if (List.length(export_excepts) > 0) {
            Doc.concat([
              Doc.text("except "),
              Doc.join(
                Doc.comma,
                List.map(
                  (excpt: Parsetree.export_except) =>
                    switch (excpt) {
                    | ExportExceptData(data) => Doc.text(data.txt)
                    | ExportExceptValue(value) => Doc.text(value.txt)
                    },
                  export_excepts,
                ),
              ),
            ]);
          } else {
            Doc.nil;
          },
        ])
      };

    Doc.concat([
      stmtLeadingCommentDocs,
      blankLineAbove,
      attributeText,
      Doc.group(withoutComments),
    ]);
  };
};

let reformat_ast =
    (parsed_program: Parsetree.parsed_program, original_source: list(string)) => {
  let _ =
    Walktree.walktree(parsed_program.statements, parsed_program.comments);

  let lastStmt = ref(None);
  let previousLine = ref(0);
  let printedDoc =
    Doc.join(
      Doc.hardLine,
      List.map(
        (stmt: Grain_parsing.Parsetree.toplevel_stmt) => {
          let line =
            toplevel_print(
              ~data=stmt,
              ~original_source,
              ~previousLine=previousLine^,
            );

          previousLine := getEndLocLine(stmt.ptop_loc);
          lastStmt := Some(stmt);
          line;
        },
        parsed_program.statements,
      ),
    );

  let (_leadingComments, trailingComments) =
    switch (lastStmt^) {
    | None => ([], [])
    | Some(stmt) => Walktree.partitionComments(stmt.ptop_loc, None)
    };

  let trailingCommentDocs =
    if (List.length(trailingComments) > 0) {
      Doc.concat([
        print_multi_comments(trailingComments, previousLine^),
        Doc.hardLine,
      ]);
    } else {
      Doc.nil;
    };

  let finalDoc = Doc.concat([printedDoc, trailingCommentDocs]);

  // Use this to check the generated output
  //Doc.debug(finalDoc);
  //

  Doc.toString(~width=80, finalDoc) |> print_endline;
  //use this to see the AST in JSON
  // print_endline(
  //   Yojson.Basic.pretty_to_string(
  //     Yojson.Safe.to_basic(
  //       Grain_parsing__Parsetree.parsed_program_to_yojson(parsed_program),
  //     ),
  //   ),
  // );
};
