/**
 * PhpNoiseGenerator.cpp
 *
 * Exposes pocketmine\level\generator\normal\noise\NoiseGeneratorOctaves
 * as a native C++ class to PHP.
 *
 * Drop-in replacement for the PHP class used in Aquamarine's Normal generator.
 * The PHP class lives at:
 *   src/pocketmine/level/generator/normal/noise/NoiseGeneratorOctaves.php
 * and internally uses NoiseGeneratorImproved.php.
 *
 * By registering this C++ class under the same namespace/name,
 * PHP will use it automatically if the extension is loaded —
 * no changes to Aquamarine's PHP code needed.
 */

#include "lib/ImprovedNoise.h"
#include "ZendUtil.h"

extern "C" {
#include "php.h"
#include "Zend/zend_exceptions.h"
#include "ext/spl/spl_exceptions.h"
}

/* ---- internal object struct ---- */

typedef struct {
    NoiseGeneratorOctaves *generator; // heap-allocated
    zend_object std;
} noise_generator_obj;

static zend_class_entry *noise_generator_entry;
static zend_object_handlers noise_generator_handlers;

static inline noise_generator_obj *fetch_noise_obj(zend_object *obj) {
    return (noise_generator_obj *)((char *)obj - XtOffsetOf(noise_generator_obj, std));
}

/* ---- zend object lifecycle ---- */

static zend_object *noise_generator_new(zend_class_entry *ce) {
    noise_generator_obj *obj = (noise_generator_obj *)
        ecalloc(1, sizeof(noise_generator_obj) + zend_object_properties_size(ce));
    obj->generator = nullptr;
    zend_object_std_init(&obj->std, ce);
    object_properties_init(&obj->std, ce);
    obj->std.handlers = &noise_generator_handlers;
    return &obj->std;
}

static void noise_generator_free(zend_object *obj) {
    noise_generator_obj *intern = fetch_noise_obj(obj);
    if (intern->generator) {
        delete intern->generator;
        intern->generator = nullptr;
    }
    zend_object_std_dtor(obj);
}

/* ---- PHP methods ---- */

/**
 * __construct($seed, int $octavesIn)
 * Matches: NoiseGeneratorOctaves::__construct($seed, int $octavesIn)
 * $seed can be an integer or a CustomRandom object; we only support integer seeds
 * (the Normal generator always passes an integer: $this->random->getSeed() or similar).
 * If an object is passed, we extract its internal seed via a property read.
 */
PHP_METHOD(pocketmine_level_generator_normal_noise_NoiseGeneratorOctaves, __construct) {
    zval *seedZval;
    zend_long octaves;

    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 2, 2)
        Z_PARAM_ZVAL(seedZval)
        Z_PARAM_LONG(octaves)
    ZEND_PARSE_PARAMETERS_END();

    int64_t seed = 0;

    if (Z_TYPE_P(seedZval) == IS_LONG) {
        seed = (int64_t)Z_LVAL_P(seedZval);
    } else if (Z_TYPE_P(seedZval) == IS_OBJECT) {
        // Try to read a 'seed' property from the object (CustomRandom has ->seed)
        zval *prop = zend_read_property(Z_OBJCE_P(seedZval), Z_OBJ_P(seedZval),
                                         "seed", sizeof("seed") - 1, 1, nullptr);
        if (prop && Z_TYPE_P(prop) == IS_LONG) {
            seed = (int64_t)Z_LVAL_P(prop);
        }
        // If the object has a getSeed() method, call it
        else {
            zval retval;
            zend_string *method_name = zend_string_init("getSeed", sizeof("getSeed") - 1, 0);
            if (zend_hash_exists(&Z_OBJCE_P(seedZval)->function_table, method_name)) {
                call_user_function(NULL, seedZval, zend_string_to_zval(method_name, &retval),
                                   &retval, 0, NULL);
                if (Z_TYPE(retval) == IS_LONG) seed = (int64_t)Z_LVAL(retval);
                zval_ptr_dtor(&retval);
            }
            zend_string_release(method_name);
        }
    } else {
        convert_to_long(seedZval);
        seed = (int64_t)Z_LVAL_P(seedZval);
    }

    noise_generator_obj *intern = fetch_noise_obj(Z_OBJ_P(getThis()));
    if (intern->generator) {
        delete intern->generator;
    }
    intern->generator = new NoiseGeneratorOctaves(seed, (int)octaves);
}

/**
 * generateNoiseOctaves8(array &$noiseArray, int $xOffset, int $zOffset,
 *                        int $xSize, int $zSize,
 *                        float $xScale, float $zScale, float $p10) : array
 */
PHP_METHOD(pocketmine_level_generator_normal_noise_NoiseGeneratorOctaves, generateNoiseOctaves8) {
    zval *noiseArrayZval;
    zend_long xOffset, zOffset, xSize, zSize;
    double xScale, zScale, p10;

    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 8, 8)
        Z_PARAM_ARRAY_EX(noiseArrayZval, 0, 1)
        Z_PARAM_LONG(xOffset)
        Z_PARAM_LONG(zOffset)
        Z_PARAM_LONG(xSize)
        Z_PARAM_LONG(zSize)
        Z_PARAM_DOUBLE(xScale)
        Z_PARAM_DOUBLE(zScale)
        Z_PARAM_DOUBLE(p10)
    ZEND_PARSE_PARAMETERS_END();

    noise_generator_obj *intern = fetch_noise_obj(Z_OBJ_P(getThis()));
    if (!intern->generator) {
        zend_throw_exception(spl_ce_RuntimeException, "NoiseGeneratorOctaves not initialized", 0);
        return;
    }

    std::vector<double> noiseVec;
    intern->generator->generateNoiseOctaves8(noiseVec,
        (int)xOffset, (int)zOffset, (int)xSize, (int)zSize,
        xScale, zScale, p10);

    // Write back into the PHP array reference
    zval_ptr_dtor(noiseArrayZval);
    array_init_size(noiseArrayZval, (uint32_t)noiseVec.size());
    for (size_t i = 0; i < noiseVec.size(); i++) {
        add_next_index_double(noiseArrayZval, noiseVec[i]);
    }

    // Return same array (PHP side does $result = $this->depthNoise->generateNoiseOctaves8(...))
    ZVAL_COPY(return_value, noiseArrayZval);
}

/**
 * generateNoiseOctaves(array $noiseArray, int $xOffset, int $yOffset, int $zOffset,
 *                       int $xSize, int $ySize, int $zSize,
 *                       float $xScale, float $yScale, float $zScale) : array
 */
PHP_METHOD(pocketmine_level_generator_normal_noise_NoiseGeneratorOctaves, generateNoiseOctaves) {
    zval *noiseArrayZval;
    zend_long xOffset, yOffset, zOffset, xSize, ySize, zSize;
    double xScale, yScale, zScale;

    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 10, 10)
        Z_PARAM_ARRAY_EX(noiseArrayZval, 0, 1)
        Z_PARAM_LONG(xOffset)
        Z_PARAM_LONG(yOffset)
        Z_PARAM_LONG(zOffset)
        Z_PARAM_LONG(xSize)
        Z_PARAM_LONG(ySize)
        Z_PARAM_LONG(zSize)
        Z_PARAM_DOUBLE(xScale)
        Z_PARAM_DOUBLE(yScale)
        Z_PARAM_DOUBLE(zScale)
    ZEND_PARSE_PARAMETERS_END();

    noise_generator_obj *intern = fetch_noise_obj(Z_OBJ_P(getThis()));
    if (!intern->generator) {
        zend_throw_exception(spl_ce_RuntimeException, "NoiseGeneratorOctaves not initialized", 0);
        return;
    }

    std::vector<double> noiseVec;
    intern->generator->generateNoiseOctaves(noiseVec,
        (int)xOffset, (int)yOffset, (int)zOffset,
        (int)xSize, (int)ySize, (int)zSize,
        xScale, yScale, zScale);

    zval_ptr_dtor(noiseArrayZval);
    array_init_size(noiseArrayZval, (uint32_t)noiseVec.size());
    for (size_t i = 0; i < noiseVec.size(); i++) {
        add_next_index_double(noiseArrayZval, noiseVec[i]);
    }

    ZVAL_COPY(return_value, noiseArrayZval);
}

/* ---- argument info ---- */

ZEND_BEGIN_ARG_INFO_EX(arginfo_noise_construct, 0, 0, 2)
    ZEND_ARG_INFO(0, seed)
    ZEND_ARG_TYPE_INFO(0, octavesIn, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_noise_gen8, 0, 0, 8)
    ZEND_ARG_INFO(1, noiseArray)
    ZEND_ARG_TYPE_INFO(0, xOffset, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, zOffset, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, xSize,   IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, zSize,   IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, xScale,  IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, zScale,  IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, p10,     IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_noise_gen, 0, 0, 10)
    ZEND_ARG_INFO(1, noiseArray)
    ZEND_ARG_TYPE_INFO(0, xOffset, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, yOffset, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, zOffset, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, xSize,   IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, ySize,   IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, zSize,   IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, xScale,  IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, yScale,  IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, zScale,  IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry noise_generator_methods[] = {
    PHP_ME(pocketmine_level_generator_normal_noise_NoiseGeneratorOctaves, __construct,        arginfo_noise_construct, ZEND_ACC_PUBLIC)
    PHP_ME(pocketmine_level_generator_normal_noise_NoiseGeneratorOctaves, generateNoiseOctaves8, arginfo_noise_gen8,   ZEND_ACC_PUBLIC)
    PHP_ME(pocketmine_level_generator_normal_noise_NoiseGeneratorOctaves, generateNoiseOctaves,  arginfo_noise_gen,    ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void register_noise_generator_class() {
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce,
        "pocketmine\\level\\generator\\normal\\noise\\NoiseGeneratorOctaves",
        noise_generator_methods);
    noise_generator_entry = zend_register_internal_class(&ce);
    noise_generator_entry->ce_flags |= ZEND_ACC_FINAL;

    memcpy(&noise_generator_handlers, zend_get_std_object_handlers(),
           sizeof(zend_object_handlers));
    noise_generator_handlers.offset = XtOffsetOf(noise_generator_obj, std);
    noise_generator_handlers.free_obj = noise_generator_free;
}
