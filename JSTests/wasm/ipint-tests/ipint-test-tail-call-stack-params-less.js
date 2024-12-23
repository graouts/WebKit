import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $test (export "test")
        (param i32)
        (result i32)
    
        (i32.const 1)
        (i32.const 2)
        (i32.const 3)
        (i32.const 4)
        (i32.const 5)
        (i32.const 6)
        (i32.const 7)
        (i32.const 8)
        (i32.const 9)
        (i32.const 10)
        (i32.const 11)
        (i32.const 12)
    
        (call $caller
            (i32.const 20)
            (i32.const 21)
            (i32.const 22)
            (i32.const 23)
            (i32.const 24)
            (i32.const 25)
            (i32.const 26)
            (i32.const 27)
            (i32.const 28)
            (i32.const 29)
            (i32.const 30)
        )

        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
    )

    (func $caller
        (param i32)
        (param i32)
        (param i32)
        (param i32)
        (param i32)
        (param i32)
        (param i32)
        (param i32)
        (param i32)
        (param i32)
        (param i32)

        (result i32)

        (return_call $callee
            (local.get 0)
            (local.get 1)
            (local.get 2)
            (local.get 3)
            (local.get 4)
            (local.get 5)
            (local.get 6)
            (local.get 7)
            (local.get 8)
        )
    )

    (func $callee
        (param i32)
        (param i32)
        (param i32)
        (param i32)
        (param i32)
        (param i32)
        (param i32)
        (param i32)
        (param i32)

        (result i32)

        (i32.const 40)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { tail_call: true });
    const { test } = instance.exports
    assert.eq(test(), 118);
}

await assert.asyncTest(test())
